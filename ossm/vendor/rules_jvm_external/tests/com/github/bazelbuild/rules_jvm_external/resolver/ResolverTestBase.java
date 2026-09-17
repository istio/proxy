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

package com.github.bazelbuild.rules_jvm_external.resolver;

import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.cmd.ResolverConfig;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.LogEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.DownloadResult;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.Downloader;
import com.github.bazelbuild.rules_jvm_external.resolver.ui.NullListener;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.graph.Graph;
import com.google.common.hash.Hashing;
import com.google.gson.Gson;
import com.sun.net.httpserver.BasicAuthenticator;
import com.sun.net.httpserver.HttpContext;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;
import org.apache.maven.model.Dependency;
import org.apache.maven.model.DependencyManagement;
import org.apache.maven.model.Model;
import org.apache.maven.model.Parent;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public abstract class ResolverTestBase {

  @Rule public TemporaryFolder tempFolder = new TemporaryFolder();

  protected Resolver resolver;
  protected DelegatingListener listener = new DelegatingListener();

  protected abstract Resolver getResolver(Netrc netrc, EventListener listener);

  @Before
  public void createResolver() {
    resolver = getResolver(Netrc.create(null, new HashMap<>()), listener);
  }

  @Test
  public void shouldResolveASingleCoordinate() {
    Coordinates coords = new Coordinates("com.example:foo:1.0");

    Path repo = MavenRepo.create().add(coords).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), coords)).getResolution();

    assertEquals(1, resolved.nodes().size());
    assertEquals(coords, resolved.nodes().iterator().next());
  }

  @Test
  public void shouldResolveASingleCoordinateWithADep() {
    Coordinates main = new Coordinates("com.example:foo:1.0");
    Coordinates dep = new Coordinates("com.example:bar:1.0");

    Path repo = MavenRepo.create().add(dep).add(main, dep).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), main)).getResolution();

    Set<Coordinates> nodes = resolved.nodes();
    assertEquals(2, nodes.size());
    assertEquals(ImmutableSet.of(dep), resolved.successors(main));
  }

  @Test
  public void shouldFetchTransitiveDependencies() {
    Coordinates grandDep = new Coordinates("com.example:grand-dep:1.0");
    Coordinates dep = new Coordinates("com.example:dep:2.0");
    Coordinates main = new Coordinates("com.example:main:3.0");

    Path repo = MavenRepo.create().add(grandDep).add(dep, grandDep).add(main, dep).getPath();

    Graph<Coordinates> graph =
        resolver.resolve(prepareRequestFor(repo.toUri(), main)).getResolution();

    Set<Coordinates> nodes = graph.nodes();
    assertTrue(nodes.contains(main));
    assertTrue(nodes.contains(dep));
    assertTrue(nodes.contains(grandDep));
  }

  @Test
  public void shouldAllowExclusionsAtTheCoordinateLevel() {
    Coordinates singleDep = new Coordinates("com.example:single-dep:1.0");
    Coordinates noDeps = new Coordinates("com.example:no-deps:1.0");

    Path repo = MavenRepo.create().add(noDeps).add(singleDep, noDeps).getPath();

    ResolutionRequest request =
        prepareRequestFor(repo.toUri() /* deliberately left blank */)
            .addArtifact(singleDep.toString(), "com.example:no-deps");

    Graph<Coordinates> graph = resolver.resolve(request).getResolution();

    Set<Coordinates> nodes = graph.nodes();
    assertFalse(nodes.contains(noDeps));
    assertTrue(nodes.contains(singleDep));
  }

  @Test
  public void shouldAllowGlobalExclusions() {
    Coordinates singleDep = new Coordinates("com.example:single-dep:1.0");
    Coordinates noDeps = new Coordinates("com.example:no-deps:1.0");

    Path repo = MavenRepo.create().add(noDeps).add(singleDep, noDeps).getPath();

    ResolutionRequest request =
        prepareRequestFor(repo.toUri(), singleDep).exclude("com.example:no-deps");

    Graph<Coordinates> graph = resolver.resolve(request).getResolution();

    Set<Coordinates> nodes = graph.nodes();
    assertFalse(nodes.contains(noDeps));
    assertTrue(nodes.contains(singleDep));
  }

  @Test
  public void shouldBeAbleToFetchCoordinatesWhichDifferByClassifier() {
    Coordinates jar = new Coordinates("com.example:thing:1.2.3");
    Coordinates classified = new Coordinates("com.example:thing:jar:sausages:1.2.3");

    Path repo = MavenRepo.create().add(jar).add(classified).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), jar, classified)).getResolution();

    Set<Coordinates> nodes = resolved.nodes();
    assertEquals(2, nodes.size());
    assertTrue(nodes.toString(), nodes.contains(jar));
    assertTrue(nodes.toString(), nodes.contains(classified));
  }

  @Test
  public void shouldDownloadJarsOverHttp() throws IOException {
    Coordinates coords = new Coordinates("com.example:foo:1.0");

    Path repo = MavenRepo.create().add(coords).getPath();

    HttpServer server = HttpServer.create(new InetSocketAddress("localhost", 0), 0);
    server.createContext("/", new PathHandler(repo));
    server.start();

    int port = server.getAddress().getPort();
    URI remote = URI.create("http://localhost:" + port);

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(remote, coords)).getResolution();

    assertEquals(1, resolved.nodes().size());
    assertEquals(coords, resolved.nodes().iterator().next());
  }

  @Test
  public void shouldDownloadOverHttpWithAuthenticationPassedInOnRepoUrl() throws IOException {
    Coordinates coords = new Coordinates("com.example:foo:1.0");

    Path repo = MavenRepo.create().add(coords).getPath();

    HttpServer server = HttpServer.create(new InetSocketAddress("localhost", 0), 0);
    HttpContext context = server.createContext("/", new PathHandler(repo));
    context.setAuthenticator(
        new BasicAuthenticator("maven") {
          @Override
          public boolean checkCredentials(String username, String password) {
            return "cheese".equals(username) && "hunter2".equals(password);
          }
        });
    server.start();

    int port = server.getAddress().getPort();

    URI remote = URI.create("http://cheese:hunter2@localhost:" + port);

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(remote, coords)).getResolution();

    assertEquals(1, resolved.nodes().size());
    assertEquals(coords, resolved.nodes().iterator().next());
  }

  @Test
  public void shouldDownloadOverHttpWithAuthenticationGatheredFromNetrc() throws IOException {
    Netrc netrc =
        new Netrc(new Netrc.Credential("localhost", "cheese", "hunter2", null), new HashMap<>());
    assertAuthenticatedAccessWorks(netrc, "cheese", "hunter2");
  }

  @Test
  public void shouldDownloadOverHttpWithMachineSpecificAuthenticationFromNetrc()
      throws IOException {
    Netrc netrc =
        new Netrc(
            null,
            ImmutableMap.of(
                "localhost", new Netrc.Credential("localhost", "cheese", "hunter2", null)));
    assertAuthenticatedAccessWorks(netrc, "cheese", "hunter2");
  }

  @Test
  public void shouldHandlePackagingPomsInDependencies() throws IOException {
    Coordinates parentCoords = new Coordinates("com.example:packaging:1.0.3");
    Model parent = createModel(parentCoords);
    parent.setPackaging("pom");

    Coordinates first = new Coordinates("com.example:first:1.2.3");
    Coordinates second = new Coordinates("com.example:second:1.2.3");

    Path repo = MavenRepo.create().add(first).add(second).add(parent, first, second).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), parentCoords)).getResolution();

    // We don't want nodes with a classifier of `pom` to have appeared in the graph
    assertEquals(Set.of(first, second, parentCoords), resolved.nodes());

    Path localRepo = Files.createTempDirectory("local");

    DownloadResult parentDownload =
        new Downloader(
                Netrc.fromUserHome(),
                localRepo,
                Set.of(repo.toUri()),
                new NullListener(),
                false,
                Map.of())
            .download(parentCoords);

    assertTrue(parentDownload.getPath().isEmpty());
  }

  @Test
  public void shouldAssumeAPackagingPomIsAJarWhenDependedOnTransitively() {
    Coordinates grandParent = new Coordinates("com.example:grandparent:3.14.1");

    Coordinates parentCoords = new Coordinates("com.example:packaging:1.0.3");
    Model parent = createModel(parentCoords);
    parent.setPackaging("pom");

    Coordinates first = new Coordinates("com.example:first:1.2.3");
    Coordinates second = new Coordinates("com.example:second:1.2.3");

    Path repo =
        MavenRepo.create()
            .add(first)
            .add(second)
            .add(parent, first, second)
            // We want the "parentCoords" to be referenced with a `type` element of `pom`. Within
            // `MavenRepo` we can do that by setting the parent coordinate's extension.
            .add(
                grandParent,
                new Coordinates(
                    parentCoords.getGroupId(),
                    parentCoords.getArtifactId(),
                    "pom",
                    null,
                    parentCoords.getVersion()))
            .getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), grandParent)).getResolution();

    assertEquals(Set.of(grandParent, parentCoords, first, second), resolved.nodes());
  }

  @Test
  public void packagingAttributeOfPomShouldBeRespected() throws IOException {
    Coordinates coords = new Coordinates("com.example:packaging:1.0.3");
    Model model = createModel(coords);
    model.setPackaging("aar");

    Path repo = MavenRepo.create().add(model).writePomFile(model).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), coords)).getResolution();
    assertEquals(1, resolved.nodes().size());

    Coordinates resolvedCoords = resolved.nodes().iterator().next();
    assertEquals("aar", resolvedCoords.getExtension());
  }

  @Test
  public void bundlePackagingShouldBeRewrittenToJar() throws IOException {
    Coordinates coords = new Coordinates("com.example:bundle-artifact:2.0.1");
    Model model = createModel(coords);
    model.setPackaging("bundle");

    // Create a jar file in the repo, but the POM declares packaging as "bundle"
    Coordinates jarCoords = new Coordinates("com.example:bundle-artifact:jar:2.0.1");
    Path repo = MavenRepo.create().add(jarCoords).writePomFile(model).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), coords)).getResolution();
    assertEquals(1, resolved.nodes().size());

    Coordinates resolvedCoords = resolved.nodes().iterator().next();
    assertEquals("jar", resolvedCoords.getExtension());
  }

  @Test
  public void shouldResolveAndDownloadItemIdentifiedByClassifierFromArgsFile() throws IOException {
    Map<String, Object> args =
        Map.of(
            "artifacts",
            List.of(
                Map.of(
                    "artifact", "artifact",
                    "group", "com.example",
                    "version", "7.8.9",
                    "classifier", "jdk15")));
    Path argsFile = tempFolder.newFolder("argsdir").toPath().resolve("config.json");
    Files.write(argsFile, new Gson().toJson(args).getBytes(UTF_8));

    Coordinates coords = new Coordinates("com.example", "artifact", null, "jdk15", "7.8.9");
    Path repo = MavenRepo.create().add(coords).getPath();

    ResolverConfig config =
        new ResolverConfig(new NullListener(), "--argsfile", argsFile.toAbsolutePath().toString());
    ResolutionRequest request = config.getResolutionRequest();
    request.addRepository(repo.toUri());

    Graph<Coordinates> resolved = resolver.resolve(request).getResolution();

    assertEquals(resolved.nodes(), Set.of(coords));
  }

  @Test
  public void shouldNotCrashWhenPomFileIsIncorrect() {
    // This example is derived from org.apache.yetus:audience-annotations:0.11.0
    Coordinates coords = new Coordinates("com.example:bad-dep:123.1");
    Model model = createModel(coords);
    Dependency jdkDep = new Dependency();
    jdkDep.setGroupId("jdk.tools");
    jdkDep.setArtifactId("jdk.tools");
    jdkDep.setScope("system");
    jdkDep.setOptional("true");
    model.addDependency(jdkDep);

    Path repo = MavenRepo.create().add(model).getPath();
    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), coords)).getResolution();

    assertEquals(Set.of(coords), resolved.nodes());
  }

  @Test
  public void shouldIncludeFullDependencyGraphWithoutRemovingDuplicateEntries() {
    Coordinates sharedDep = new Coordinates("com.example:shared:7.8.9");
    Coordinates first = new Coordinates("com.example:first:1.2.3");
    Coordinates second = new Coordinates("com.example:second:3.4.5");

    Path repo =
        MavenRepo.create().add(sharedDep).add(first, sharedDep).add(second, sharedDep).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), first, second)).getResolution();
    assertEquals(3, resolved.nodes().size());

    Set<Coordinates> firstSuccessors = resolved.successors(first);
    assertEquals(Set.of(sharedDep), firstSuccessors);

    Set<Coordinates> secondSuccessors = resolved.successors(second);
    assertEquals(Set.of(sharedDep), secondSuccessors);
  }

  @Test
  public void managedDependenciesOfTransitiveDepsDoOverrideRegularDependencies() {
    // Modelled after `org.drools:drools-mvel:7.53.0.Final`. Resolved using
    // maven, the dependency graph looks like:
    //
    // [INFO] com.example:foo:jar:1.0.4
    // [INFO] \- org.drools:drools-mvel:jar:7.53.0.Final:compile
    // [INFO]    +- org.mvel:mvel2:jar:2.4.12.Final:compile
    // [INFO]    +- org.kie:kie-api:jar:7.53.0.Final:compile
    // [INFO]    |  \- org.kie.soup:kie-soup-maven-support:jar:7.53.0.Final:compile
    // [INFO]    +- org.kie:kie-internal:jar:7.53.0.Final:compile
    // [INFO]    +- org.kie.soup:kie-soup-project-datamodel-commons:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.kie.soup:kie-soup-commons:jar:7.53.0.Final:compile
    // [INFO]    |  \- org.kie.soup:kie-soup-project-datamodel-api:jar:7.53.0.Final:compile
    // [INFO]    +- org.drools:drools-core:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.kie.soup:kie-soup-xstream:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.drools:drools-core-reflective:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.drools:drools-core-dynamic:jar:7.53.0.Final:runtime
    // [INFO]    |  \- commons-codec:commons-codec:jar:1.11:compile
    // [INFO]    +- org.drools:drools-compiler:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.kie:kie-memory-compiler:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.drools:drools-ecj:jar:7.53.0.Final:compile
    // [INFO]    |  +- org.antlr:antlr-runtime:jar:3.5.2:compile
    // [INFO]    |  \- com.thoughtworks.xstream:xstream:jar:1.4.16:compile
    // [INFO]    |     \- io.github.x-stream:mxparser:jar:1.2.1:compile
    // [INFO]    |        \- xmlpull:xmlpull:jar:1.1.3.1:compile
    // [INFO]    \- org.slf4j:slf4j-api:jar:1.7.30:compile
    //
    // `xmlpull:xmlpull` is defined as needing version `1.1.3.1` in the
    // `xstream` deps, but the `org.kie` dependencies have a
    // `managedDependencies` section which mandates version `1.2.0` which is
    // not in Maven Central.

    // Let's build the models from the bottom to the top
    Coordinates leafCoords = new Coordinates("com.example:managed:1.2.0");

    Coordinates parentCoords = new Coordinates("org.kie:parent:1.2.0");
    Model parentModel = createModel(parentCoords);
    parentModel.setPackaging("pom");
    DependencyManagement managedDeps = new DependencyManagement();
    Dependency overriddenVersionDep = new Dependency();
    overriddenVersionDep.setGroupId(leafCoords.getGroupId());
    overriddenVersionDep.setArtifactId(leafCoords.getArtifactId());
    // The differing version will break things, so assert this
    overriddenVersionDep.setVersion("3.14.1");
    assertNotEquals(overriddenVersionDep.getVersion(), leafCoords.getVersion());
    managedDeps.addDependency(overriddenVersionDep);
    parentModel.setDependencyManagement(managedDeps);

    Coordinates childCoords = new Coordinates("org.kie:child:1.2.0");
    Model childModel = createModel(childCoords);
    Parent childsParent = new Parent();
    childsParent.setGroupId(parentCoords.getGroupId());
    childsParent.setArtifactId(parentCoords.getArtifactId());
    childsParent.setVersion(parentCoords.getVersion());
    childModel.setParent(childsParent);

    Path repo =
        MavenRepo.create().add(leafCoords).add(parentModel).add(childModel, leafCoords).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), childCoords)).getResolution();
    assertEquals(Set.of(childCoords, leafCoords), resolved.nodes());
  }

  @Test
  public void shouldIndicateWhenConflictsAreDetected() {
    Coordinates olderVersion = new Coordinates("com.example:child:1.0");
    Coordinates dependsOnOlder = new Coordinates("com.example:foo:2.0");
    Coordinates newerVersion = new Coordinates("com.example:child:1.5");
    Coordinates dependsOnNewer = new Coordinates("com.example:bar:4.2");
    Coordinates root = new Coordinates("com.example:root:1.0");

    Path repo =
        MavenRepo.create()
            .add(newerVersion)
            .add(dependsOnNewer, newerVersion)
            .add(olderVersion)
            .add(dependsOnOlder, olderVersion)
            .add(root, dependsOnNewer, dependsOnOlder)
            .getPath();

    ResolutionResult result = resolver.resolve(prepareRequestFor(repo.toUri(), root));
    Set<Conflict> conflicts = result.getConflicts();

    assertEquals(Set.of(new Conflict(newerVersion, olderVersion)), conflicts);
  }

  @Test
  public void conflictsAreIgnoredIfSpecifiedInSetOfArtifactsToResolve() {
    Coordinates olderVersion = new Coordinates("com.example:child:1.0");
    Coordinates dependsOnOlder = new Coordinates("com.example:foo:2.0");
    Coordinates newerVersion = new Coordinates("com.example:child:1.5");
    Coordinates dependsOnNewer = new Coordinates("com.example:bar:4.2");
    Coordinates root = new Coordinates("com.example:root:1.0");

    Path repo =
        MavenRepo.create()
            .add(newerVersion)
            .add(dependsOnNewer, newerVersion)
            .add(olderVersion)
            .add(dependsOnOlder, olderVersion)
            .add(root, dependsOnNewer, dependsOnOlder)
            .getPath();

    ResolutionResult result = resolver.resolve(prepareRequestFor(repo.toUri(), root, newerVersion));

    Set<Conflict> conflicts = result.getConflicts();
    assertEquals(Set.of(), conflicts);

    assertTrue(result.getResolution().nodes().contains(newerVersion));
  }

  @Test
  public void shouldConvergeToASingleVersionOfADependency() {
    Coordinates olderVersion = new Coordinates("com.example:child:1.0");
    Coordinates dependsOnOlder = new Coordinates("com.example:foo:2.0");
    Coordinates newerVersion = new Coordinates("com.example:child:1.5");
    Coordinates dependsOnNewer = new Coordinates("com.example:bar:4.2");
    Coordinates root = new Coordinates("com.example:root:1.0");

    Path repo =
        MavenRepo.create()
            .add(newerVersion)
            .add(dependsOnNewer, newerVersion)
            .add(olderVersion)
            .add(dependsOnOlder, olderVersion)
            .add(root, dependsOnNewer, dependsOnOlder)
            .getPath();

    Graph<Coordinates> resolution =
        resolver.resolve(prepareRequestFor(repo.toUri(), root)).getResolution();

    // We don't care whether we have version 1.0 or 1.5 (different resolvers
    // might pick different versions), but there should only be one version
    // of this dependency.
    Set<Coordinates> children =
        resolution.nodes().stream()
            .filter(c -> "child".equals(c.getArtifactId()))
            .collect(Collectors.toSet());

    assertEquals(children.toString(), 1, children.size());
  }

  @Test
  public void
      shouldNormalizeVersionToHighestVersionIfPomsAskForDifferentClassifiersWithDifferentVersions() {
    // This behaviour is exhibited by both `coursier` and `gradle`, so we're
    // going to settle on this as what's expected. `maven` will quite happily
    // (and correctly IMO) include the javadoc dep at its version and the jar
    // dep at its version. The problem is that when we come to render the lock
    // file we can't have two items with the same `groupId:artifactId` with
    // different versions, so I guess that's another reason to normalise the
    // weird thing that the other resolvers do.

    // The default coordinate has a lower version number than the coordinate
    // with the classifier.
    Coordinates base = new Coordinates("com.example:odd-deps:1.0.0");
    Coordinates classified = base.setClassifier("javadoc").setVersion("18.0");
    Coordinates depOnBase = new Coordinates("com.example:base:4.0.0");
    Coordinates depOnClassified = new Coordinates("com.example:diff:2.0.0");

    Path repo =
        MavenRepo.create()
            .add(base)
            .add(classified)
            .add(depOnBase, base)
            .add(depOnClassified, classified)
            .getPath();

    Graph<Coordinates> resolution =
        resolver
            .resolve(prepareRequestFor(repo.toUri(), depOnBase, depOnClassified))
            .getResolution();
    Set<Coordinates> nodes = resolution.nodes();

    assertTrue("javadoc with higher version number not found", nodes.contains(classified));
    assertTrue(
        "regular jar with higher version number not found",
        nodes.contains(classified.setClassifier(null))); // Same version, no classifer
  }

  @Test
  public void shouldWarnOnMissingTransitiveDependencies() {
    List<LogEvent> logEvents = new ArrayList<>();
    listener.addListener(
        event -> {
          if (event instanceof LogEvent) {
            logEvents.add((LogEvent) event);
          }
        });

    Coordinates present = new Coordinates("com.example:present:1.0.0");
    Coordinates missing = new Coordinates("com.example:missing:1.0.0");
    Path repo = MavenRepo.create().add(present, missing).getPath();

    Graph<Coordinates> resolution =
        resolver.resolve(prepareRequestFor(repo.toUri(), present)).getResolution();
    assertEquals(Set.of(present, missing), resolution.nodes());

    if (!logEvents.stream()
        .filter(e -> e.toString().contains("The POM for " + missing.setExtension("pom")))
        .findFirst()
        .isPresent()) {
      throw new AssertionError("Cannot find expected log message");
    }
  }

  @Test
  public void shouldPriortizedVersionsfromBomFilesInOrder() {
    Coordinates jacksonCoreCoords = new Coordinates("com.fasterxml.jackson.core:jackson-core");

    Coordinates jacksonBom13 = new Coordinates("com.fasterxml.jackson:jackson-bom:2.13.5");
    Dependency dependency13 = new Dependency();
    dependency13.setGroupId(jacksonCoreCoords.getGroupId());
    dependency13.setArtifactId(jacksonCoreCoords.getArtifactId());
    dependency13.setVersion("2.13.5");
    DependencyManagement managedDeps13 = new DependencyManagement();
    managedDeps13.addDependency(dependency13);
    Model model13 = createModel(jacksonBom13);
    model13.setPackaging("pom");
    model13.setDependencyManagement(managedDeps13);

    Coordinates jacksonBom14 = new Coordinates("com.fasterxml.jackson:jackson-bom:2.14.3");
    Dependency dependency14 = new Dependency();
    dependency14.setGroupId(jacksonCoreCoords.getGroupId());
    dependency14.setArtifactId(jacksonCoreCoords.getArtifactId());
    dependency14.setVersion("2.14.3");
    DependencyManagement managedDeps14 = new DependencyManagement();
    managedDeps14.addDependency(dependency14);
    Model model14 = createModel(jacksonBom14);
    model14.setPackaging("pom");
    model14.setDependencyManagement(managedDeps14);

    Coordinates jacksonBom16 = new Coordinates("com.fasterxml.jackson:jackson-bom:2.16.2");
    Dependency dependency16 = new Dependency();
    dependency16.setGroupId(jacksonCoreCoords.getGroupId());
    dependency16.setArtifactId(jacksonCoreCoords.getArtifactId());
    dependency16.setVersion("2.16.2");
    DependencyManagement managedDeps16 = new DependencyManagement();
    managedDeps16.addDependency(dependency16);
    Model model16 = createModel(jacksonBom16);
    model16.setPackaging("pom");
    model16.setDependencyManagement(managedDeps16);

    Coordinates coordinates = new Coordinates("com.example:bomordertest:1.0.0");
    Model model = createModel(coordinates);
    Dependency dependency = new Dependency();
    dependency.setGroupId(jacksonCoreCoords.getGroupId());
    dependency.setArtifactId(jacksonCoreCoords.getArtifactId());
    model.addDependency(dependency);

    Path repo =
        MavenRepo.create().add(model13).add(model14).add(model16).add(model, coordinates).getPath();

    ResolutionRequest request = prepareRequestFor(repo.toUri(), jacksonCoreCoords);
    request.addBom(jacksonBom14);
    request.addBom(jacksonBom16);
    request.addBom(jacksonBom13);

    Graph<Coordinates> resolved = resolver.resolve(request).getResolution();
    assertEquals(
        Set.of(new Coordinates("com.fasterxml.jackson.core:jackson-core:2.14.3")),
        resolved.nodes());
  }

  @Test
  public void shouldConsolidateDifferentClassifierVersionsForADependency() {
    Coordinates nettyCoords = new Coordinates("io.netty:netty-tcnative-boringssl-static");

    Coordinates nettyOsxBom =
        new Coordinates("io.netty:netty-tcnative-boringssl-static:osx-aarch_64:2.0.47.Final");
    Dependency dependencyOsx = new Dependency();
    dependencyOsx.setGroupId(nettyCoords.getGroupId());
    dependencyOsx.setArtifactId(nettyCoords.getArtifactId());
    dependencyOsx.setClassifier("osx-aarch_64");
    dependencyOsx.setVersion("2.0.47.Final");
    DependencyManagement managedDepsOsx = new DependencyManagement();
    managedDepsOsx.addDependency(dependencyOsx);
    Model modelOsx = createModel(nettyOsxBom);
    modelOsx.setPackaging("pom");
    modelOsx.setDependencyManagement(managedDepsOsx);

    Coordinates nettyBom = new Coordinates("io.netty:netty-tcnative-boringssl-static:2.0.60.Final");
    Dependency dependencyNetty = new Dependency();
    dependencyNetty.setGroupId(nettyCoords.getGroupId());
    dependencyNetty.setArtifactId(nettyCoords.getArtifactId());
    dependencyNetty.setVersion("2.0.60.Final");
    DependencyManagement managedDepsNetty = new DependencyManagement();
    managedDepsNetty.addDependency(dependencyNetty);
    Model modelNetty = createModel(nettyBom);
    modelNetty.setPackaging("pom");
    modelNetty.setDependencyManagement(managedDepsNetty);

    Coordinates coordinates = new Coordinates("com.example:bomclassifiertest:1.0.0");
    Model model = createModel(coordinates);
    Dependency dependency = new Dependency();
    dependency.setGroupId(nettyCoords.getGroupId());
    dependency.setArtifactId(nettyCoords.getArtifactId());
    model.addDependency(dependency);

    Path repo = MavenRepo.create().add(modelOsx).add(modelNetty).add(model, coordinates).getPath();

    ResolutionRequest request = prepareRequestFor(repo.toUri(), nettyCoords);
    request.addBom(nettyOsxBom);
    request.addBom(nettyBom);

    Graph<Coordinates> resolved = resolver.resolve(request).getResolution();
    assertEquals(
        Set.of(new Coordinates("io.netty:netty-tcnative-boringssl-static:2.0.47.Final")),
        resolved.nodes());
  }

  @Test
  public void shouldContractRelocatedPomButKeepAggregatorPom() {
    // Relocated artifact: old:relocated -> new:target
    Coordinates relocated = new Coordinates("com.example:relocated:1.0.0");
    Coordinates target = new Coordinates("com.example:target:1.0.0");

    // Aggregator POM with two children should remain in the graph
    Coordinates aggregator = new Coordinates("com.example:aggregator:9.9.9");
    Model aggregatorModel = createModel(aggregator);
    aggregatorModel.setPackaging("pom");

    Coordinates childA = new Coordinates("com.example:childA:1.2.3");
    Coordinates childB = new Coordinates("com.example:childB:4.5.6");

    Path repo =
        MavenRepo.create()
            // target and its jar
            .add(target)
            // relocated POM that points to target
            .addRelocation(relocated, target)
            // aggregator POM and its children
            .add(childA)
            .add(childB)
            .add(aggregatorModel, childA, childB)
            .getPath();

    // Ask to resolve both the relocated coordinate and the aggregator coordinate
    Graph<Coordinates> graph =
        resolver.resolve(prepareRequestFor(repo.toUri(), relocated, aggregator)).getResolution();

    // The relocated coordinate should be contracted to the target, so the graph should contain
    // the target instead of the relocated origin.
    assertTrue(graph.nodes().contains(target));
    assertFalse(graph.nodes().contains(relocated));

    // The aggregator POM should stay as a node with edges to both children
    assertTrue(graph.nodes().contains(aggregator));
    assertEquals(Set.of(childA, childB), graph.successors(aggregator));
  }

  @Test
  public void shouldHandleRelocationWithVersionConflict() {
    // This test reproduces the mysql-connector-java issue where:
    // - mysql:mysql-connector-java:8.0.33 is a relocation POM pointing to
    // com.mysql:mysql-connector-j
    // - When both the old coordinate (8.0.33) and new coordinate (8.4.0) are requested,
    // - The resolver should handle the version conflict and pick the higher version

    // The old artifact that has been relocated
    Coordinates oldCoord = new Coordinates("mysql:mysql-connector-java:8.0.33");
    // The new artifact at the same version as the relocation
    Coordinates newCoordSameVersion = new Coordinates("com.mysql:mysql-connector-j:8.0.33");
    // The new artifact at a higher version
    Coordinates newCoordHigherVersion = new Coordinates("com.mysql:mysql-connector-j:8.4.0");

    Path repo =
        MavenRepo.create()
            // Add the new coordinate at both versions
            .add(newCoordSameVersion)
            .add(newCoordHigherVersion)
            // Add the relocation POM: old -> new (at same version)
            .addRelocation(oldCoord, newCoordSameVersion)
            .getPath();

    // Request both the old coordinate and the new coordinate at different version
    Graph<Coordinates> graph =
        resolver
            .resolve(prepareRequestFor(repo.toUri(), oldCoord, newCoordHigherVersion))
            .getResolution();

    // The resolver should:
    // 1. Follow the relocation from mysql:mysql-connector-java:8.0.33 to
    // com.mysql:mysql-connector-j:8.0.33
    // 2. Resolve the version conflict between 8.0.33 and 8.4.0
    // 3. Pick the higher version (8.4.0)
    // The graph should only contain the higher version of the new coordinate
    assertEquals(Set.of(newCoordHigherVersion), graph.nodes());
  }

  protected Model createModel(Coordinates coords) {
    Model model = new Model();
    model.setModelVersion("4.0.0");

    model.setGroupId(coords.getGroupId());
    model.setArtifactId(coords.getArtifactId());
    model.setVersion(coords.getVersion());

    return model;
  }

  private void assertAuthenticatedAccessWorks(Netrc netrc, String user, String password)
      throws IOException {
    Coordinates coords = new Coordinates("com.example:foo:3.4.5");

    Path repo = MavenRepo.create().add(coords).getPath();

    HttpServer server = HttpServer.create(new InetSocketAddress("localhost", 0), 0);
    HttpContext context = server.createContext("/", new PathHandler(repo));
    context.setAuthenticator(
        new BasicAuthenticator("maven") {
          @Override
          public boolean checkCredentials(String username, String pwd) {
            if (user == null) {
              return true;
            }
            return user.equals(username) && password.equals(pwd);
          }
        });
    server.start();

    int port = server.getAddress().getPort();

    URI remote = URI.create("http://localhost:" + port);

    Resolver resolver = getResolver(netrc, new NullListener());
    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(remote, coords)).getResolution();

    assertEquals(1, resolved.nodes().size());
    assertEquals(coords, resolved.nodes().iterator().next());
  }

  @Test
  public void shouldRespectForceVersionWhenResolvingConflicts() throws IOException {
    // When force_version is set on a lower version, it should win over the higher version
    // that would normally be selected during version conflict resolution.
    Coordinates lowerVersion = new Coordinates("com.example:forced:1.0");
    Coordinates higherVersion = new Coordinates("com.example:forced:2.0");
    Coordinates dependsOnLower = new Coordinates("com.example:uses-lower:1.0");
    Coordinates dependsOnHigher = new Coordinates("com.example:uses-higher:1.0");

    Path repo =
        MavenRepo.create()
            .add(lowerVersion)
            .add(higherVersion)
            .add(dependsOnLower, lowerVersion)
            .add(dependsOnHigher, higherVersion)
            .getPath();

    // Create a request that forces the lower version using the new forceVersion capability
    ResolutionRequest request = new ResolutionRequest().addRepository(repo.toUri());
    request.addArtifact(dependsOnLower.toString());
    // Force the lower version by creating an Artifact with forceVersion=true
    Artifact forcedArtifact = new Artifact(lowerVersion, Set.of(), true);
    request.addArtifact(forcedArtifact);
    request.addArtifact(dependsOnHigher.toString());

    // Without force_version implementation, this test will fail for Gradle resolver
    // because it will resolve to the higher version (2.0) instead of the forced lower version (1.0)
    ResolutionResult result = resolver.resolve(request);
    Graph<Coordinates> graph = result.getResolution();

    Set<Coordinates> forcedNodes =
        graph.nodes().stream()
            .filter(c -> "forced".equals(c.getArtifactId()))
            .collect(Collectors.toSet());

    assertEquals("Should resolve to exactly one version", 1, forcedNodes.size());

    assertTrue(
        "Should resolve to forced lower version when force_version is set",
        forcedNodes.contains(lowerVersion));

    // Validate that the downloaded artifact has the correct SHA for the forced version.
    // This catches bugs where the resolver fetches artifacts via detached configurations
    // that bypass force_version, resulting in the wrong file being downloaded.
    Map<Coordinates, Path> paths = result.getPaths();
    if (paths.containsKey(lowerVersion)) {
      Path downloadedArtifact = paths.get(lowerVersion);
      Path expectedArtifact = repo.resolve(lowerVersion.toRepoPath());

      // Compare SHA256 of downloaded file vs expected file in repo
      byte[] downloadedBytes = Files.readAllBytes(downloadedArtifact);
      byte[] expectedBytes = Files.readAllBytes(expectedArtifact);
      String downloadedSha = Hashing.sha256().hashBytes(downloadedBytes).toString();
      String expectedSha = Hashing.sha256().hashBytes(expectedBytes).toString();

      assertEquals(
          "Downloaded artifact SHA should match the forced version's artifact",
          expectedSha,
          downloadedSha);
    }
  }

  @Test
  public void shouldIncludeRuntimeScopedDependencies() throws IOException {
    // Dependencies with runtime scope should be included in the resolution graph.
    // This tests that the resolver uses runtime classpath, not just compile/api classpath.
    Coordinates main = new Coordinates("com.example:main:1.0");
    Coordinates runtimeDep = new Coordinates("com.example:runtime-only:1.0");

    // Create a POM where main depends on runtime-only with runtime scope
    Model mainModel = createModel(main);
    Dependency dep = new Dependency();
    dep.setGroupId(runtimeDep.getGroupId());
    dep.setArtifactId(runtimeDep.getArtifactId());
    dep.setVersion(runtimeDep.getVersion());
    dep.setScope("runtime");
    mainModel.addDependency(dep);

    Path repo = MavenRepo.create().add(runtimeDep).add(mainModel, main).getPath();

    Graph<Coordinates> resolved =
        resolver.resolve(prepareRequestFor(repo.toUri(), main)).getResolution();

    assertTrue(
        "Runtime-scoped dependency should be included in resolution",
        resolved.nodes().contains(runtimeDep));
    assertTrue("Main artifact should be in resolution", resolved.nodes().contains(main));
  }

  protected ResolutionRequest prepareRequestFor(URI repo, Coordinates... coordinates) {
    ResolutionRequest request = new ResolutionRequest().addRepository(repo);
    for (Coordinates coordinate : coordinates) {
      request = request.addArtifact(coordinate.toString());
    }
    return request;
  }
}
