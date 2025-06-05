This folder contains test data.

The utf8 folder contains a set of files whose names are not ASCII or ISO-8859-1.
They may be used for testing the ability to handle files using non ASCII
file names. Along with those we have 4 samples of what native tar and zip
utilities do with these file names:

- utf8_linux.tar:  From linux: `tar cf utf8_linux.tar utf8`
- utf8_linux.zip From linux: `zip -r utf8_linux.zip utf8`
- utf8_mac.tar:  From macos: `tar cf utf8_linux.tar utf8`
- utf8_mac.zip From macos: `zip -r utf8_linux.zip utf8`
- utf8_win.tar:  From window: `tar cf utf8_win.tar utf8`
- utf8_win.zip From window: `7z a -r utf8_win.zip utf8`

The samples are are intended to be used as input data for tests
of capabilities that read data (such as unpacking and filtering
a tar file). Code must be able to read tar and zip files produced
on a different OS and interpret file names correctly.  For now we
can study the differences by platform.

