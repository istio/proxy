# PyPI integration code

This code is for integrating with PyPI and other compatible indexes. At the
moment we have code for:
* Downloading packages using `pip` or `repository_ctx.download`.
* Interacting with PyPI compatible indexes via [SimpleAPI] spec.
* Locking a `requirements.in` or [PEP621] compliant `pyproject.toml`.

[PEP621]: https://peps.python.org/pep-0621/
