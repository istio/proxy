package com.github.bazelbuild.rules_jvm_external.maven;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.math.BigInteger;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Base64;
import java.util.logging.Logger;
import org.bouncycastle.bcpg.ArmoredOutputStream;
import org.bouncycastle.bcpg.BCPGOutputStream;
import org.bouncycastle.openpgp.PGPCompressedData;
import org.bouncycastle.openpgp.PGPException;
import org.bouncycastle.openpgp.PGPObjectFactory;
import org.bouncycastle.openpgp.PGPPrivateKey;
import org.bouncycastle.openpgp.PGPPublicKey;
import org.bouncycastle.openpgp.PGPSecretKey;
import org.bouncycastle.openpgp.PGPSignature;
import org.bouncycastle.openpgp.PGPSignatureGenerator;
import org.bouncycastle.openpgp.PGPSignatureList;
import org.bouncycastle.openpgp.PGPUtil;
import org.bouncycastle.openpgp.jcajce.JcaPGPSecretKeyRing;
import org.bouncycastle.openpgp.operator.PBESecretKeyDecryptor;
import org.bouncycastle.openpgp.operator.bc.BcKeyFingerprintCalculator;
import org.bouncycastle.openpgp.operator.bc.BcPBESecretKeyDecryptorBuilder;
import org.bouncycastle.openpgp.operator.bc.BcPGPContentSignerBuilder;
import org.bouncycastle.openpgp.operator.bc.BcPGPContentVerifierBuilderProvider;
import org.bouncycastle.openpgp.operator.bc.BcPGPDigestCalculatorProvider;

public class MavenSigning {
  private static final int BUFFER = 4096;
  private static final Logger LOG = Logger.getLogger(MavenSigning.class.getName());

  /**
   * This method is based on the following Gradle code: <a
   * href="https://cs.android.com/android-studio/gradle/+/master:platforms/software/security/src/main/java/org/gradle/security/internal/pgp/BaseInMemoryPgpSignatoryProvider.java">...</a>
   * <a
   * href="https://cs.android.com/android-studio/gradle/+/master:platforms/software/security/src/main/java/org/gradle/plugins/signing/signatory/pgp/PgpSignatory.java">...</a>
   * <a
   * href="https://cs.android.com/android-studio/gradle/+/master:platforms/software/dependency-management/src/main/java/org/gradle/api/internal/artifacts/ivyservice/ivyresolve/verification/writer/WriteDependencyVerificationFile.java;l=586?q=ArmoredOutputStream&ss=android-studio%2Fgradle">...</a>
   * <a
   * href="https://cs.android.com/android-studio/gradle/+/master:platforms/software/security/src/main/java/org/gradle/security/internal/SecuritySupport.java;l=65">...</a>
   *
   * @param toSign
   * @param key
   * @param password
   * @return
   * @throws IOException
   */
  protected static Path in_memory_pgp_sign(Path toSign, String key, String password)
      throws IOException {
    LOG.info("Signing " + toSign + " with in-memory PGP keys");

    Path dir = Files.createTempDirectory("maven-sign");
    Path signatureOutputPath = dir.resolve(toSign.getFileName() + ".asc");

    PGPSecretKey pgpSecretKey;
    // CI builder should have the key in the environment variable in Base64 format
    try (InputStream in =
        PGPUtil.getDecoderStream(new ByteArrayInputStream(Base64.getDecoder().decode(key)))) {
      pgpSecretKey = new JcaPGPSecretKeyRing(in).getSecretKey();
    } catch (Exception e) {
      throw new IOException("Could not read PGP secret key", e);
    }

    // Decrypt the PGPSecretKey to get a PGPPrivateKey
    PGPPrivateKey privateKey = createPrivateKey(pgpSecretKey, password);

    try {
      // Create a PGPSignatureGenerator with the PGPPrivateKey
      PGPSignatureGenerator signatureGenerator =
          new PGPSignatureGenerator(
              new BcPGPContentSignerBuilder(
                  pgpSecretKey.getPublicKey().getAlgorithm(), PGPUtil.SHA512));
      signatureGenerator.init(PGPSignature.BINARY_DOCUMENT, privateKey);

      // Read the input file and write its contents to the PGPSignatureGenerator
      try (InputStream in = new BufferedInputStream(Files.newInputStream(toSign))) {
        int ch;
        while ((ch = in.read()) >= 0) {
          signatureGenerator.update((byte) ch);
        }
      }

      // Generate the signature
      PGPSignature signature = signatureGenerator.generate();

      // Write the signature to the output file
      try (ArmoredOutputStream out =
          new ArmoredOutputStream(
              new BCPGOutputStream(Files.newOutputStream(signatureOutputPath)))) {
        signature.encode(out, true /* forTransfer */);
        out.flush();
      }
    } catch (PGPException e) {
      throw new RuntimeException(e);
    }

    // Verify the signature
    PGPSignatureList pgpSignatures = readSignatures(signatureOutputPath);
    if (pgpSignatures == null) {
      throw new IOException("Could not read PGP signatures");
    }

    for (PGPSignature signature : pgpSignatures) {
      try {
        if (!verify(toSign, signature, pgpSecretKey.getPublicKey())) {
          throw new RuntimeException(
              String.format("Could not verify PGP signature for file %s!", toSign.getFileName()));
        } else {
          LOG.info(String.format("PGP signature verified for file %s", toSign.getFileName()));
        }
      } catch (PGPException e) {
        throw new RuntimeException(e);
      }
    }

    return signatureOutputPath;
  }

  private static String bytesToHex(byte[] bytes) {
    return String.format("%0" + (bytes.length << 1) + "x", new BigInteger(1, bytes));
  }

  private static boolean verify(Path originFilePath, PGPSignature signature, PGPPublicKey publicKey)
      throws PGPException {
    signature.init(new BcPGPContentVerifierBuilderProvider(), publicKey);

    LOG.info(
        String.format(
            "[Verify] Public Key Fingerprint: %s", bytesToHex(publicKey.getFingerprint())));

    byte[] buffer = new byte[BUFFER];
    int len;
    try (InputStream in = new BufferedInputStream(Files.newInputStream(originFilePath))) {
      while ((len = in.read(buffer)) >= 0) {
        signature.update(buffer, 0, len);
      }
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    return signature.verify();
  }

  private static PGPSignatureList readSignatures(Path signaturePath) {
    try (InputStream stream = new BufferedInputStream(Files.newInputStream(signaturePath));
        InputStream decoderStream = PGPUtil.getDecoderStream(stream)) {
      return readSignatureList(decoderStream);
    } catch (IOException | PGPException e) {
      throw new RuntimeException(e);
    }
  }

  private static PGPSignatureList readSignatureList(InputStream decoderStream)
      throws IOException, PGPException {
    PGPObjectFactory objectFactory =
        new PGPObjectFactory(decoderStream, new BcKeyFingerprintCalculator());
    Object nextObject = objectFactory.nextObject();
    if (nextObject instanceof PGPSignatureList) {
      return (PGPSignatureList) nextObject;
    } else if (nextObject instanceof PGPCompressedData) {
      return readSignatureList(((PGPCompressedData) nextObject).getDataStream());
    } else {
      LOG.warning(
          String.format(
              "Skip parsing signature list in %s.",
              nextObject == null ? "invalid file" : nextObject.getClass()));
      return null;
    }
  }

  private static PGPPrivateKey createPrivateKey(PGPSecretKey secretKey, String password) {
    try {
      PBESecretKeyDecryptor decryptor =
          new BcPBESecretKeyDecryptorBuilder(new BcPGPDigestCalculatorProvider())
              .build(password.toCharArray());
      return secretKey.extractPrivateKey(decryptor);
    } catch (PGPException e) {
      throw new RuntimeException("Could not extract private key", e);
    }
  }

  protected static Path gpg_sign(Path toSign) throws IOException, InterruptedException {
    LOG.info("Signing " + toSign + " with GPG");

    // Ideally, we'd use BouncyCastle for this, but for now brute force by assuming
    // the gpg binary is on the path

    Path dir = Files.createTempDirectory("maven-sign");
    Path file = dir.resolve(toSign.getFileName() + ".asc");

    Process gpg =
        new ProcessBuilder(
                "gpg",
                "--use-agent",
                "--armor",
                "--detach-sign",
                "--no-tty",
                "-o",
                file.toAbsolutePath().toString(),
                toSign.toAbsolutePath().toString())
            .inheritIO()
            .start();
    gpg.waitFor();
    if (gpg.exitValue() != 0) {
      throw new IllegalStateException("Unable to sign: " + toSign);
    }

    // Verify the signature
    Process verify =
        new ProcessBuilder(
                "gpg",
                "--verify",
                "--verbose",
                "--verbose",
                file.toAbsolutePath().toString(),
                toSign.toAbsolutePath().toString())
            .inheritIO()
            .start();
    verify.waitFor();
    if (verify.exitValue() != 0) {
      throw new IllegalStateException("Unable to verify signature of " + toSign);
    }

    return file;
  }

  protected enum SigningMethod {
    GPG,
    PGP,
    NONE
  }

  public static class SigningMetadata {
    private final String signingKey;
    private final String signingPassword;
    protected final SigningMethod signingMethod;

    protected static SigningMetadata noSigner() {
      return new SigningMetadata(false, false, null, null);
    }

    protected SigningMetadata(
        boolean gpgSign, boolean useInMemoryPgpKeys, String signingKey, String signingPassword) {
      this.signingKey = signingKey;
      this.signingPassword = signingPassword;
      if (gpgSign && useInMemoryPgpKeys) {
        throw new IllegalArgumentException("Cannot use in-memory PGP keys with GPG signing");
      }
      if (gpgSign) {
        this.signingMethod = SigningMethod.GPG;
      } else if (useInMemoryPgpKeys) {
        this.signingMethod = SigningMethod.PGP;
      } else {
        this.signingMethod = SigningMethod.NONE;
      }
    }

    public String getSigningKey() {
      return signingKey;
    }

    public String getSigningPassword() {
      return signingPassword;
    }
  }
}
