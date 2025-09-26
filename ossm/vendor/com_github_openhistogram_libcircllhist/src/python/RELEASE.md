# Release procedure

* Edit setup.py and increase version number

* Commit the change with a message:

        Version bump to $version

* Upload to pypi with:

        ./setup.py sdist upload
