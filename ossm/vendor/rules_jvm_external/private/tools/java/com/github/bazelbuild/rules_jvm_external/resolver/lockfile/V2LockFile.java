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

package com.github.bazelbuild.rules_jvm_external.resolver.lockfile;

import static com.google.common.base.StandardSystemProperty.USER_HOME;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.Conflict;
import com.github.bazelbuild.rules_jvm_external.resolver.DependencyInfo;
import com.google.gson.Gson;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.file.Paths;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.stream.Collectors;

/** Format resolution results into the v2 lock file format. */
public class V2LockFile {

  public static final URI M2_LOCAL_URI =
      Paths.get(USER_HOME.value()).resolve(".m2/repository").toUri();
  private final Collection<URI> allRepos;
  private final Set<DependencyInfo> infos;
  private final Set<Conflict> conflicts;

  public V2LockFile(
      Collection<URI> repositories, Set<DependencyInfo> infos, Set<Conflict> conflicts) {
    this.allRepos = repositories;
    this.infos = infos;
    this.conflicts = conflicts;
  }

  public Collection<URI> getRepositories() {
    return allRepos;
  }

  public Set<DependencyInfo> getDependencyInfos() {
    return infos;
  }

  public Set<Conflict> getConflicts() {
    return conflicts;
  }

  @SuppressWarnings("unchecked")
  public static V2LockFile create(String from) {
    Map<?, ?> raw = new Gson().fromJson(from, Map.class);

    Set<URI> repos = new LinkedHashSet<>();
    Map<String, Collection<String>> allRepos =
        (Map<String, Collection<String>>) raw.get("repositories");
    if (allRepos == null) {
      allRepos = Map.of();
    }
    allRepos.keySet().stream().map(URI::create).forEach(repos::add);
    if (Boolean.TRUE.equals(raw.get("m2local"))) {
      repos.add(M2_LOCAL_URI);
    }

    // Get all the coordinates out of the lock file
    Map<Coordinates, String> coords2Shasum = new LinkedHashMap<>();
    Map<String, Coordinates> key2Coords = new LinkedHashMap<>();
    Map<?, ?> artifactsMap = (Map<?, ?>) raw.get("artifacts");
    if (artifactsMap == null) {
      artifactsMap = Map.of();
    }
    for (Map.Entry<?, ?> entry : artifactsMap.entrySet()) {
      String key = entry.getKey().toString();

      Map<?, ?> entryData = (Map<?, ?>) entry.getValue();
      String version = (String) entryData.get("version");

      String[] parts = key.split(":", 3);
      Coordinates baseCoords =
          parts.length == 2
              ? new Coordinates(parts[0], parts[1], null, null, version)
              : new Coordinates(parts[0], parts[1], parts[3], null, version);

      Map<?, ?> shasums = (Map<?, ?>) entryData.get("shasums");
      if (shasums == null) {
        shasums = Map.of();
      }
      for (Map.Entry<?, ?> shasum : shasums.entrySet()) {
        if (shasum.getValue() != null) {
          Coordinates newCoords = baseCoords.setClassifier((String) shasum.getKey());
          coords2Shasum.put(newCoords, (String) shasum.getValue());
          key2Coords.put(newCoords.asKey(), newCoords);
        }
      }
    }

    // Map dependencies back
    Map<String, Collection<String>> rawDependencies =
        (Map<String, Collection<String>>) raw.get("dependencies");
    if (rawDependencies == null) {
      rawDependencies = Map.of();
    }
    Map<Coordinates, Set<Coordinates>> dependencies = new LinkedHashMap<>();
    for (Map.Entry<String, Collection<String>> entry : rawDependencies.entrySet()) {
      Coordinates coords = key2Coords.get(entry.getKey());
      if (coords == null) {
        System.err.println("Unable to find mapping for " + entry.getKey());
        continue;
      }
      for (String depKey : entry.getValue()) {
        Coordinates depCoords = key2Coords.get(depKey);
        if (depCoords == null) {
          System.err.println("Unable to find mapping for " + depKey);
          continue;
        }
        dependencies.computeIfAbsent(coords, k -> new HashSet<>()).add(depCoords);
      }
    }

    // Now find out which repositories contain which artifacts
    Map<Coordinates, Set<URI>> coords2Repos = new LinkedHashMap<>();
    for (Map.Entry<String, Collection<String>> entry : allRepos.entrySet()) {
      URI repo = URI.create(entry.getKey());
      for (Coordinates coords : coords2Shasum.keySet()) {
        if (entry.getValue().contains(coords.asKey())) {
          coords2Repos.computeIfAbsent(coords, k -> new HashSet<>()).add(repo);
        }
      }
    }

    // And now we can recreate the `DependencyInfo`s
    Set<DependencyInfo> infos = new HashSet<>();
    for (Map.Entry<Coordinates, String> entry : coords2Shasum.entrySet()) {
      Coordinates coords = entry.getKey();
      infos.add(
          new DependencyInfo(
              coords,
              coords2Repos.get(coords),
              Optional.empty(),
              Optional.of(entry.getValue()),
              dependencies.getOrDefault(coords, Set.of()),
              Set.of(),
              new TreeMap<>()));
    }

    // Finally, gather the conflicts
    Set<Conflict> conflicts = new HashSet<>();
    Map<String, String> rawConflicts = (Map<String, String>) raw.get("conflict_resolution");
    if (rawConflicts == null) {
      rawConflicts = Map.of();
    }
    for (Map.Entry<String, String> entry : rawConflicts.entrySet()) {
      Coordinates requested = new Coordinates(entry.getKey());
      Coordinates resolved = new Coordinates(entry.getValue());
      conflicts.add(new Conflict(resolved, requested));
    }

    return new V2LockFile(repos, infos, conflicts);
  }

  /** "Render" the resolution result to a `Map` suitable for printing as JSON. */
  public Map<String, Object> render() {
    Set<URI> repositories = new LinkedHashSet<>(allRepos);

    boolean isUsingM2Local = repositories.stream().anyMatch(M2_LOCAL_URI::equals);

    Map<String, Map<String, Object>> artifacts = new TreeMap<>();
    Map<String, Set<String>> deps = new TreeMap<>();
    Map<String, Set<String>> packages = new TreeMap<>();
    Map<String, Map<String, SortedSet<String>>> services = new TreeMap<>();
    Map<String, Set<String>> repos = new LinkedHashMap<>();
    repositories.stream()
        .filter(r -> !M2_LOCAL_URI.equals(r))
        .forEach(r -> repos.put(stripAuthenticationInformation(r), new TreeSet<>()));

    Set<String> skipped = new TreeSet<>();
    Map<String, String> files = new TreeMap<>();

    infos.forEach(
        info -> {
          Coordinates coords = info.getCoordinates();
          String key = coords.asKey();

          // The short key is the group:artifact[:extension] tuple. The classifier
          // is used as a key in the shasum dict, and the version is also stored
          // in the same dict as the shasums. In the common case where we have
          // multiple `jar` artifacts, this means that we group all the classifiers
          // together.
          String shortKey = coords.getGroupId() + ":" + coords.getArtifactId();
          if (coords.getExtension() != null
              && !coords.getExtension().isEmpty()
              && !"jar".equals(coords.getExtension())) {
            shortKey += ":" + coords.getExtension();
          }

          if (info.getPath().isEmpty() || info.getSha256().isEmpty()) {
            skipped.add(key);
          }

          Map<String, Object> artifactValue =
              artifacts.computeIfAbsent(shortKey, k -> new TreeMap<>());
          artifactValue.put("version", coords.getVersion());

          String classifier;
          if (coords.getClassifier() == null || coords.getClassifier().isEmpty()) {
            classifier = "jar";
          } else {
            classifier = coords.getClassifier();
          }
          @SuppressWarnings("unchecked")
          Map<String, String> shasums =
              (Map<String, String>) artifactValue.computeIfAbsent("shasums", k -> new TreeMap<>());
          info.getSha256().ifPresent(sha -> shasums.put(classifier, sha));

          info.getRepositories()
              .forEach(
                  repo -> {
                    repos
                        .getOrDefault(stripAuthenticationInformation(repo), new TreeSet<>())
                        .add(key);
                  });

          deps.put(
              key,
              info.getDependencies().stream()
                  .map(Coordinates::asKey)
                  .map(Object::toString)
                  .collect(Collectors.toCollection(TreeSet::new)));
          packages.put(key, info.getPackages());
          services.put(key, info.getServices());

          if (info.getPath().isPresent()) {
            // Regularise paths to UNIX format
            files.put(key, info.getPath().get().toString().replace('\\', '/'));
          }
        });

    Map<String, Object> lock = new LinkedHashMap<>();
    lock.put("artifacts", ensureArtifactsAllHaveAtLeastOneShaSum(artifacts));
    lock.put("dependencies", removeEmptyItems(deps));
    lock.put("packages", removeEmptyItems(packages));
    lock.put("services", removeEmptyItemsMap(services));
    if (isUsingM2Local) {
      lock.put("m2local", true);
    }
    // Use a treemap to sort the repo map by keys in the lock file
    lock.put("repositories", new TreeMap<>(repos));

    lock.put("skipped", skipped);
    if (conflicts != null && !conflicts.isEmpty()) {
      Map<String, String> renderedConflicts = new TreeMap<String, String>();
      for (Conflict conflict : conflicts) {
        renderedConflicts.put(
            conflict.getRequested().toString(), conflict.getResolved().toString());
      }

      lock.put("conflict_resolution", renderedConflicts);
    }
    lock.put("files", files);

    lock.put("version", "2");

    return lock;
  }

  private Map<String, Map<String, Object>> ensureArtifactsAllHaveAtLeastOneShaSum(
      Map<String, Map<String, Object>> artifacts) {
    for (Map<String, Object> item : artifacts.values()) {
      @SuppressWarnings("unchecked")
      Map<String, Object> shasums = (Map<String, Object>) item.get("shasums");
      if (shasums == null || !shasums.isEmpty()) {
        continue;
      }
      shasums.put("jar", null);
    }
    return artifacts;
  }

  private <K, V extends Collection> Map<K, V> removeEmptyItems(Map<K, V> input) {
    return input.entrySet().stream()
        .filter(e -> !e.getValue().isEmpty())
        .collect(
            Collectors.toMap(
                Map.Entry::getKey,
                Map.Entry::getValue,
                (l, r) -> {
                  l.addAll(r);
                  return l;
                },
                TreeMap::new));
  }

  private <K, VK, VV> Map<K, Map<VK, VV>> removeEmptyItemsMap(Map<K, Map<VK, VV>> input) {
    return input.entrySet().stream()
        .filter(e -> !e.getValue().isEmpty())
        .collect(
            Collectors.toMap(
                Map.Entry::getKey,
                Map.Entry::getValue,
                (l, r) -> {
                  l.putAll(r);
                  return l;
                },
                TreeMap::new));
  }

  private String stripAuthenticationInformation(URI uri) {
    try {
      URI stripped =
          new URI(
              uri.getScheme(),
              null,
              uri.getHost(),
              uri.getPort(),
              uri.getPath(),
              uri.getQuery(),
              uri.getFragment());

      String toReturn = stripped.toString();

      if (stripped.getQuery() == null && uri.getFragment() == null && !toReturn.endsWith("/")) {
        toReturn += "/";
      }

      return toReturn;
    } catch (URISyntaxException e) {
      // Do nothing: we may not have been given a URI, but something like `m2local/`
    }
    return uri.toString();
  }
}
