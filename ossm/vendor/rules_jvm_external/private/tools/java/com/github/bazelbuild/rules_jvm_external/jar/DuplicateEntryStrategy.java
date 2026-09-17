package com.github.bazelbuild.rules_jvm_external.jar;

import static java.util.stream.Collectors.joining;

import java.io.IOException;
import java.util.Arrays;

enum DuplicateEntryStrategy {
  LAST_IN_WINS("last-wins") {
    @Override
    public boolean isReplacingCurrent(String name, byte[] originalHash, byte[] newHash) {
      return true;
    }
  },
  FIRST_IN_WINS("first-wins") {
    @Override
    public boolean isReplacingCurrent(String name, byte[] originalHash, byte[] newHash) {
      return originalHash == null;
    }
  },
  IS_ERROR("are-errors") {
    @Override
    public boolean isReplacingCurrent(String name, byte[] originalHash, byte[] newHash)
        throws IOException {
      if (originalHash == null) {
        return true;
      }

      if (Arrays.equals(originalHash, newHash)) {
        return false;
      }

      throw new IOException("Attempt to write different duplicate file for: " + name);
    }
  };

  private final String shortName;

  DuplicateEntryStrategy(String shortName) {
    this.shortName = shortName;
  }

  public static DuplicateEntryStrategy fromShortName(String name) {
    for (DuplicateEntryStrategy value : DuplicateEntryStrategy.values()) {
      if (value.shortName.equals(name)) {
        return value;
      }
    }
    throw new IllegalArgumentException(
        String.format(
            "Unable to find matching short name for %s. Valid options are: %s",
            name, Arrays.stream(values()).map(v -> v.shortName).collect(joining(", "))));
  }

  public String toString() {
    return shortName;
  }

  /**
   * Whether the current version of {@code name} (as identified by {@code originalHash}) should be
   * replaced by the version identified by {@code newHash}. Both hashes should be generated using a
   * {@link java.security.MessageDigest}, and the algorithm used for both should be the same.
   *
   * @param originalHash Generated hash, which may be null.
   * @param newHash Generated hash, which must not be null.
   */
  public abstract boolean isReplacingCurrent(String name, byte[] originalHash, byte[] newHash)
      throws IOException;
}
