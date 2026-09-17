// Copyright 2024 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.github.bazelbuild.rules_jvm_external.resolver.cmd;

import static java.nio.charset.StandardCharsets.UTF_8;
import static java.util.stream.Collectors.joining;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.jar.IndexJar;
import com.github.bazelbuild.rules_jvm_external.jar.PerJarIndexResults;
import com.github.bazelbuild.rules_jvm_external.resolver.Conflict;
import com.github.bazelbuild.rules_jvm_external.resolver.DependencyInfo;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionRequest;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionResult;
import com.github.bazelbuild.rules_jvm_external.resolver.Resolver;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.PhaseEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.lockfile.DependencyIndex;
import com.github.bazelbuild.rules_jvm_external.resolver.lockfile.V3LockFile;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.DownloadResult;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.Downloader;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.HttpDownloader;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.UriNotFoundException;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.ImmutableSortedMap;
import com.google.common.graph.Graph;
import com.google.gson.GsonBuilder;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Supplier;

public abstract class AbstractMain {

  public void doMain(String[] args) {
    Set<DependencyInfo> infos;
    try (EventListener listener = HttpDownloader.defaultEventListener()) {
      ResolverConfig config = new ResolverConfig(listener, args);

      ResolutionRequest request = config.getResolutionRequest();

      Resolver resolver = getResolver(config.getNetrc(), config.getMaxThreads(), listener);

      ResolutionResult resolutionResult = resolver.resolve(request);

      infos = fulfillDependencyInfos(resolver, listener, config, resolutionResult);

      writeLockFile(listener, config, request, infos, resolutionResult.getConflicts());
      writeDependencyIndex(config, infos);

      System.exit(0);
    } catch (Exception e) {
      e.printStackTrace();
      System.exit(1);
    }
  }

  public abstract Resolver getResolver(Netrc netrc, int maxThreads, EventListener listener);

  private static Set<DependencyInfo> fulfillDependencyInfos(
      Resolver resolver,
      EventListener listener,
      ResolverConfig config,
      ResolutionResult resolutionResult) {
    listener.onEvent(new PhaseEvent("Downloading dependencies"));

    ResolutionRequest request = config.getResolutionRequest();
    String rjeUnsafeCache = System.getenv("RJE_UNSAFE_CACHE");
    boolean cacheResults = false;
    if (rjeUnsafeCache != null) {
      cacheResults = "1".equals(rjeUnsafeCache) || Boolean.parseBoolean(rjeUnsafeCache);
    }

    Downloader downloader =
        new Downloader(
            config.getNetrc(),
            request.getLocalCache(resolver.getName()),
            request.getRepositories(),
            listener,
            cacheResults,
            resolutionResult.getPaths());

    List<CompletableFuture<Set<DependencyInfo>>> futures = new LinkedList<>();

    Graph<Coordinates> resolved = resolutionResult.getResolution();

    ExecutorService downloadService =
        Executors.newFixedThreadPool(
            config.getMaxThreads(),
            r -> {
              Thread thread = new Thread(r);
              thread.setDaemon(true);
              thread.setName("downloader");
              return thread;
            });
    try {
      for (Coordinates coords : resolved.nodes()) {
        Supplier<Set<DependencyInfo>> dependencyInfoSupplier =
            () -> {
              try {
                return getDependencyInfos(
                    downloader,
                    coords,
                    resolved.successors(coords),
                    config.isFetchSources(),
                    config.isFetchJavadoc());
              } catch (UriNotFoundException e) {
                List<Coordinates> path = new LinkedList<>();
                path.add(coords);
                Set<Coordinates> predecessors = resolved.predecessors(coords);
                while (!predecessors.isEmpty()) {
                  Coordinates next = predecessors.iterator().next();
                  path.add(next);
                  predecessors = resolved.predecessors(next);
                }
                Collections.reverse(path);
                throw new UriNotFoundException(
                    String.format(
                        "Unable to download %s from any of %s. Required because: %s",
                        coords,
                        request.getRepositories(),
                        path.stream().map(Object::toString).collect(joining(" -> "))));
              }
            };
        futures.add(CompletableFuture.supplyAsync(dependencyInfoSupplier, downloadService));
      }

      return futures.stream()
          .map(
              future -> {
                try {
                  return future.get();
                } catch (InterruptedException e) {
                  System.exit(5);
                } catch (ExecutionException e) {
                  e.getCause().printStackTrace();
                  System.exit(2);
                }
                return null;
              })
          .flatMap(Set::stream)
          .collect(ImmutableSet.toImmutableSet());
    } finally {
      downloadService.shutdown();
    }
  }

  private static DownloadResult optionallyDownload(Downloader downloader, Coordinates coords) {
    try {
      return downloader.download(coords);
    } catch (UriNotFoundException e) {
      return null;
    }
  }

  private static Set<DependencyInfo> getDependencyInfos(
      Downloader downloader,
      Coordinates coords,
      Set<Coordinates> dependencies,
      boolean fetchSources,
      boolean fetchJavadoc) {
    ImmutableSet.Builder<DependencyInfo> toReturn = ImmutableSet.builder();

    DownloadResult result = downloader.download(coords);

    if (result == null) {
      return toReturn.build();
    }

    PerJarIndexResults indexResults;
    if (result.getPath().isPresent()) {
      try {
        indexResults = new IndexJar().index(result.getPath().get());
      } catch (IOException e) {
        throw new UncheckedIOException(e);
      }
    } else {
      indexResults = new PerJarIndexResults(new TreeSet<>(), new TreeSet<>(), new TreeMap<>());
    }

    toReturn.add(
        new DependencyInfo(
            coords,
            result.getRepositories(),
            result.getPath(),
            result.getSha256(),
            dependencies,
            indexResults.getPackages(),
            indexResults.getClasses(),
            indexResults.getServiceImplementations()));

    if (fetchSources) {
      Coordinates sourceCoords = coords.setClassifier("sources").setExtension("jar");
      DownloadResult source = optionallyDownload(downloader, sourceCoords);
      if (source != null) {
        toReturn.add(
            new DependencyInfo(
                sourceCoords,
                source.getRepositories(),
                source.getPath(),
                source.getSha256(),
                ImmutableSet.of(),
                ImmutableSet.of(),
                ImmutableSet.of(),
                ImmutableSortedMap.of()));
      }
    }

    if (fetchJavadoc) {
      Coordinates docCoords = coords.setClassifier("javadoc").setExtension("jar");
      DownloadResult javadoc = optionallyDownload(downloader, docCoords);
      if (javadoc != null) {
        toReturn.add(
            new DependencyInfo(
                docCoords,
                javadoc.getRepositories(),
                javadoc.getPath(),
                javadoc.getSha256(),
                ImmutableSet.of(),
                ImmutableSet.of(),
                ImmutableSet.of(),
                ImmutableSortedMap.of()));
      }
    }

    return toReturn.build();
  }

  private static void writeLockFile(
      EventListener listener,
      ResolverConfig config,
      ResolutionRequest request,
      Set<DependencyInfo> infos,
      Set<Conflict> conflicts)
      throws IOException {
    listener.onEvent(new PhaseEvent("Building lock file"));
    Path output = config.getOutput();

    listener.close();

    // If a dependency index is being generated, we can omit packages from the lock file
    // since that information is available in the index file
    boolean includePackages = config.getDependencyIndexOutput() == null;
    Map<String, Object> rendered =
        new V3LockFile(request.getRepositories(), infos, conflicts, includePackages).render();

    Map<Object, Object> toReturn = new TreeMap<>(rendered);
    // We don't need this, and having it will cause problems
    toReturn.remove("files");

    toReturn.put(
        "__AUTOGENERATED_FILE_DO_NOT_MODIFY_THIS_FILE_MANUALLY", "THERE_IS_NO_DATA_ONLY_ZUUL");

    if (config.getInputHash() != null) {
      toReturn.put("__INPUT_ARTIFACTS_HASH", config.getInputHash());
      toReturn.put("__RESOLVED_ARTIFACTS_HASH", calculateArtifactHash(rendered));
    }

    String converted =
        new GsonBuilder().setPrettyPrinting().serializeNulls().create().toJson(toReturn) + "\n";

    try (OutputStream os = output == null ? System.out : Files.newOutputStream(output);
        BufferedOutputStream bos = new BufferedOutputStream(os)) {
      bos.write(converted.getBytes(UTF_8));
    }
  }

  private static void writeDependencyIndex(ResolverConfig config, Set<DependencyInfo> infos)
      throws IOException {
    Path output = config.getDependencyIndexOutput();
    if (output == null) {
      return;
    }

    Map<String, Object> rendered = new DependencyIndex(infos).render();

    String converted =
        new GsonBuilder().setPrettyPrinting().serializeNulls().create().toJson(rendered) + "\n";

    try (OutputStream os = Files.newOutputStream(output);
        BufferedOutputStream bos = new BufferedOutputStream(os)) {
      bos.write(converted.getBytes(UTF_8));
    }
  }

  @SuppressWarnings("unchecked")
  public static Map<String, Integer> calculateArtifactHash(Map<String, Object> rendered) {
    Map<String, Map<String, Object>> allInfos = new LinkedHashMap<>();

    Map<String, Map<String, Object>> artifacts =
        sortMapRecursively((Map<?, ?>) rendered.get("artifacts"));
    for (Map.Entry<String, Map<String, Object>> dep : artifacts.entrySet()) {
      Map<String, Object> depInfo = dep.getValue();
      Map<String, String> shasums = (Map<String, String>) depInfo.get("shasums");

      Map<String, Object> commonInfo = new LinkedHashMap<>(depInfo);
      commonInfo.remove("shasums");

      boolean isJarType = dep.getKey().chars().filter(x -> x == ':').count() == 1;

      for (Map.Entry<String, String> shaEntry : shasums.entrySet()) {
        String type = shaEntry.getKey();
        String sha = shaEntry.getValue();

        String jarSuffix = isJarType ? ":jar" : "";
        String suffix = (!type.equals("jar")) ? jarSuffix + ":" + type : "";

        Map<String, Object> typeInfo = new LinkedHashMap<>();
        typeInfo.put("standard", commonInfo);
        typeInfo.put("sha", sha);
        allInfos.put(dep.getKey() + suffix, typeInfo);
      }
    }

    Map<String, Iterable<String>> repositories =
        sortMapRecursively((Map<?, ?>) rendered.get("repositories"));
    for (Map.Entry<String, Iterable<String>> repo : repositories.entrySet()) {
      Iterable<String> repoArtifacts = repo.getValue();
      for (String art : repoArtifacts) {
        allInfos.get(art).put("repository", repo.getKey());
      }
    }

    Map<String, Set<String>> dependencies =
        sortMapRecursively((Map<?, ?>) rendered.get("dependencies"));
    for (Map.Entry<String, Set<String>> dep : dependencies.entrySet()) {
      allInfos.get(dep.getKey()).put("dependencies", dep.getValue());
    }

    Map<String, Integer> finalHash = new TreeMap<>();
    allInfos.forEach((k, v) -> calculateFinalHash(k, allInfos, finalHash));

    return finalHash;
  }

  @SuppressWarnings("unchecked")
  private static int calculateFinalHash(
      String curr, Map<String, Map<String, Object>> allInfos, Map<String, Integer> finalHash) {
    StarlarkRepr repr = new StarlarkRepr();

    if (finalHash.containsKey(curr)) {
      return finalHash.get(curr);
    }
    if (!allInfos.containsKey(curr)) {
      return 0;
    }

    finalHash.put(curr, repr.repr(allInfos.get(curr)).hashCode());

    Set<String> deps =
        (Set<String>) allInfos.get(curr).getOrDefault("dependencies", Collections.emptySet());
    Map<String, Integer> hashedDeps = new TreeMap<>();
    for (String dep : deps) {
      hashedDeps.put(dep, calculateFinalHash(dep, allInfos, finalHash));
    }

    allInfos.get(curr).put("dependency_hashes", hashedDeps);
    finalHash.put(curr, repr.repr(allInfos.get(curr)).hashCode());
    return finalHash.get(curr);
  }

  @SuppressWarnings("unchecked")
  private static <T> Map<String, T> sortMapRecursively(Map<?, ?> map) {
    TreeMap<String, T> sorted = new TreeMap<>();
    for (Map.Entry<?, ?> entry : map.entrySet()) {
      Object value = entry.getValue();
      if (value instanceof Map) {
        value = sortMapRecursively((Map<?, ?>) value);
      } else if (value instanceof List) {
        List<?> list = (List<?>) value;
        List<Object> sortedList = new java.util.ArrayList<>();
        for (Object item : list) {
          if (item instanceof Map) {
            sortedList.add(sortMapRecursively((Map<?, ?>) item));
          } else {
            sortedList.add(item);
          }
        }
        value = sortedList;
      }
      sorted.put(String.valueOf(entry.getKey()), (T) value);
    }
    return sorted;
  }
}
