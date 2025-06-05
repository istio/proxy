package com.github.bazelbuild.rules_jvm_external.maven;

import java.io.IOException;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;
import java.util.TreeSet;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import org.apache.maven.artifact.versioning.ComparableVersion;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

public class Outdated {
  // This list came from
  // https://github.com/apache/maven/blob/master/maven-artifact/src/main/java/org/apache/maven/artifact/versioning/ComparableVersion.java#L307
  // and unfortunately ComparableVerison does not expose this in any public methods.
  private static final List<String> MAVEN_PRE_RELEASE_QUALIFIERS =
      Arrays.asList("alpha", "beta", "milestone", "cr", "rc", "snapshot");

  public static class ArtifactReleaseInfo {
    public String releaseVersion;
    public String preReleaseVersion;

    public ArtifactReleaseInfo(String releaseVersion, String preReleaseVersion) {
      this.releaseVersion = releaseVersion;
      this.preReleaseVersion = preReleaseVersion;
    }

    public boolean hasReleaseVersionGreatherThan(String version) {
      if (releaseVersion == null) {
        return false;
      } else {
        return new ComparableVersion(releaseVersion).compareTo(new ComparableVersion(version)) > 0;
      }
    }

    public boolean hasPreReleaseVersionGreatherThan(String version) {
      if (preReleaseVersion == null) {
        return false;
      } else {
        return new ComparableVersion(preReleaseVersion).compareTo(new ComparableVersion(version))
            > 0;
      }
    }
  }

  public static ArtifactReleaseInfo getReleaseVersion(
      String repository, String groupId, String artifactId) {
    String url =
        String.format(
            "%s/%s/%s/maven-metadata.xml", repository, groupId.replaceAll("\\.", "/"), artifactId);

    DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory.newInstance();
    DocumentBuilder documentBuilder;
    try {
      documentBuilder = documentBuilderFactory.newDocumentBuilder();
    } catch (ParserConfigurationException e) {
      verboseLog(String.format("Caught exception %s", e));
      return null;
    }
    Document document;
    try {
      document = documentBuilder.parse(new URL(url).openStream());
    } catch (IOException | SAXException e) {
      verboseLog(String.format("Caught exception for %s: %s", url, e));
      return null;
    }
    return getReleaseVersion(document, url);
  }

  private static boolean isPreRelease(String version) {
    String canonicalVersion = new ComparableVersion(version).getCanonical();
    verboseLog(
        String.format(
            "Checking canonical version: %s of version: %s for pre-release qualifiers",
            canonicalVersion, version));
    return MAVEN_PRE_RELEASE_QUALIFIERS.stream().anyMatch(canonicalVersion::contains);
  }

  public static ArtifactReleaseInfo getReleaseVersion(Document document, String documentUrl) {
    // example maven-metadata.xml
    // <metadata>
    //   <versioning>
    //     <latest>1.14.0-SNAPSHOT</latest>
    //     <release>1.13.0</release>
    //   </versioning>
    // </metadata>
    //
    // or
    //
    // <metadata>
    //   <groupId>javax.inject</groupId>
    //   <artifactId>javax.inject</artifactId>
    //   <version>1</version>
    //   <versioning>
    //     <versions>
    //       <version>1</version>
    //     </versions>
    //     <lastUpdated>20100720032040</lastUpdated>
    //   </versioning>
    // </metadata>
    Element metadataElement = document.getDocumentElement();
    Element versioningElement = getFirstChildElement(metadataElement, "versioning");
    if (versioningElement == null) {
      verboseLog(
          String.format(
              "Could not find <versioning> tag for %s, returning null version", documentUrl));
      return null;
    }

    String releaseVersion = null;
    String preReleaseVersion = null;
    // Note: we may want to add a flag to allow people to look for updates against
    // "latest" instead of "release"
    NodeList release = versioningElement.getElementsByTagName("release");
    if (release.getLength() > 0) {
      String version = release.item(0).getTextContent();
      if (isPreRelease(version)) {
        preReleaseVersion = version;
        verboseLog(String.format("Found pre-release version: %s", version));
      } else {
        return new ArtifactReleaseInfo(version, null);
      }
    } else {
      verboseLog(
          String.format(
              "Could not find <release> tag for %s, returning null version", documentUrl));
    }

    // If the release xml tag is missing then use the last version in the versions list.
    Element versionsElement = getFirstChildElement(versioningElement, "versions");
    if (versionsElement == null) {
      verboseLog(
          String.format(
              "Could not find <versions> tag for %s, returning null version", documentUrl));
      if (preReleaseVersion != null) {
        return new ArtifactReleaseInfo(null, preReleaseVersion);
      } else {
        return null;
      }
    }

    NodeList versions = versionsElement.getElementsByTagName("version");
    if (versions.getLength() == 0) {
      verboseLog(
          String.format("Found empty <versions> tag for %s, returning null version", documentUrl));
      if (preReleaseVersion != null) {
        return new ArtifactReleaseInfo(null, preReleaseVersion);
      } else {
        return null;
      }
    }

    TreeSet<String> sortedVersions = new TreeSet<>(Comparator.comparing(ComparableVersion::new));
    for (int i = 0; i < versions.getLength(); i++) {
      sortedVersions.add(versions.item(i).getTextContent());
    }
    for (String version : sortedVersions.descendingSet()) {
      if (!isPreRelease(version)) {
        verboseLog(String.format("Found non-pre-release version: %s", version));
        releaseVersion = version;
        break;
      }
    }

    return new ArtifactReleaseInfo(releaseVersion, preReleaseVersion);
  }

  public static Element getFirstChildElement(Element element, String tagName) {
    NodeList nodeList = element.getElementsByTagName(tagName);
    for (int i = 0; i < nodeList.getLength(); i++) {
      Node node = nodeList.item(i);
      if (node.getNodeType() == Node.ELEMENT_NODE) {
        return (Element) node;
      }
    }
    return null;
  }

  public static void verboseLog(String logline) {
    if (System.getenv("RJE_VERBOSE") != null) {
      System.out.println(logline);
    }
  }

  public static void main(String[] args) throws IOException {
    verboseLog(String.format("Running outdated with args %s", Arrays.toString(args)));

    Path artifactsFilePath = null;
    Path repositoriesFilePath = null;
    boolean useLegacyOutputFormat = false;

    for (int i = 0; i < args.length; i++) {
      switch (args[i]) {
        case "--artifacts-file":
          artifactsFilePath = Paths.get(args[++i]);
          break;

        case "--repositories-file":
          repositoriesFilePath = Paths.get(args[++i]);
          break;

        case "--legacy-output":
          useLegacyOutputFormat = true;
          break;

        default:
          throw new IllegalArgumentException(
              "Unable to parse command line: " + Arrays.toString(args));
      }
    }

    Objects.requireNonNull(artifactsFilePath, "Artifacts file must be set.");
    Objects.requireNonNull(repositoriesFilePath, "Repositories file must be set.");

    List<String> artifacts = Files.readAllLines(artifactsFilePath, StandardCharsets.UTF_8);
    List<String> repositories = Files.readAllLines(repositoriesFilePath, StandardCharsets.UTF_8);

    System.out.println(
        String.format(
            "Checking for updates of %d artifacts against the following repositories:",
            artifacts.size()));
    for (String repository : repositories) {
      System.out.println(String.format("\t%s", repository));
    }
    System.out.println();

    boolean foundUpdates = false;

    // Note: This should be straightforward to run in a thread and do multiple
    // update checks at once if we want to improve performance in the future.
    for (String artifact : artifacts) {
      if (artifact.isEmpty()) {
        continue;
      }
      String[] artifactParts = artifact.split(":");
      String groupId = artifactParts[0];
      String artifactId = artifactParts[1];
      String version = artifactParts[2];

      ArtifactReleaseInfo artifactReleaseInfo = null;
      for (String repository : repositories) {
        artifactReleaseInfo = getReleaseVersion(repository, groupId, artifactId);

        if (artifactReleaseInfo != null) {
          // We return the result from the first repository instead of searching all repositories
          // for the artifact
          verboseLog(
              String.format(
                  "Found release version [%s] and pre-release version [%s] for %s:%s in %s",
                  artifactReleaseInfo.releaseVersion,
                  artifactReleaseInfo.preReleaseVersion,
                  groupId,
                  artifactId,
                  repository));
          break;
        }
      }

      if (artifactReleaseInfo == null) {
        verboseLog(String.format("Could not find version for %s:%s", groupId, artifactId));
      } else {
        if (artifactReleaseInfo.hasPreReleaseVersionGreatherThan(version)) {
          if (useLegacyOutputFormat) {
            System.out.println(
                String.format(
                    "%s:%s [%s -> %s]",
                    groupId, artifactId, version, artifactReleaseInfo.preReleaseVersion));

          } else {
            if (artifactReleaseInfo.hasReleaseVersionGreatherThan(version)) {
              System.out.println(
                  String.format(
                      "%s:%s [%s -> %s] (pre-release: %s)",
                      groupId,
                      artifactId,
                      version,
                      artifactReleaseInfo.releaseVersion,
                      artifactReleaseInfo.preReleaseVersion));
            } else {
              System.out.println(
                  String.format(
                      "%s:%s [%s] (pre-release: %s)",
                      groupId, artifactId, version, artifactReleaseInfo.preReleaseVersion));
            }
          }
          foundUpdates = true;

        } else if (artifactReleaseInfo.hasReleaseVersionGreatherThan(version)) {
          System.out.println(
              String.format(
                  "%s:%s [%s -> %s]",
                  groupId, artifactId, version, artifactReleaseInfo.releaseVersion));
          foundUpdates = true;
        }
      }
    }

    if (!foundUpdates) {
      System.out.println("No updates found");
    }
  }
}
