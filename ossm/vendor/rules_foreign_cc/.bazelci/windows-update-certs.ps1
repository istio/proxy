cd $env:USERPROFILE;
Invoke-WebRequest https://curl.haxx.se/ca/cacert.pem -OutFile $env:USERPROFILE\cacert.pem;
$plaintext_pw = 'PASSWORD';
$secure_pw = ConvertTo-SecureString $plaintext_pw -AsPlainText -Force;
& openssl.exe pkcs12 -export -nokeys -out certs.pfx -in cacert.pem -passout pass:$plaintext_pw;
Import-PfxCertificate -Password $secure_pw  -CertStoreLocation Cert:\LocalMachine\Root -FilePath certs.pfx;
