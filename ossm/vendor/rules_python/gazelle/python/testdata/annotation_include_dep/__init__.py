import foo  # third party package
import module1

# gazelle:include_dep //foo/bar:baz
# gazelle:include_dep //hello:world,@star_wars//rebel_alliance/luke:skywalker
# gazelle:include_dep :module2

del module1
del foo
