# Copy bazel bundled cacerts and add our custom root CA.
cp $(bazel info output_base)/install/embedded_tools/jdk/lib/security/cacerts cacerts
keytool  -import -trustcacerts -file rootCA.pem -keystore cacerts -storepass changeit
