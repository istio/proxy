# Nix

Nix Flake that declares developer tools when executing in a Nix environment.

This is kept in its own package so that when using the Nix `path:` syntax, only
this directory is copied to the Nix Store instead of the entire `rules_rust`
repository.

See also: `//.direnv`
