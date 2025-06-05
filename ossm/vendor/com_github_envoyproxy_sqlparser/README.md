C++ SQL Parser Library
======================

This code has been forked from [Hyrise SQL
Parser](https://github.com/hyrise/sql-parser). Files have been moved around
and modified to make it easy for inclusion in envoy mysql_proxy. MySQL
specific extensions have been added to the SQL parser.

The SQL parser library will be eventually merged into envoy core and used
as a generic SQL parser for different database implementations (MySQL,
Postgres, etc.).
