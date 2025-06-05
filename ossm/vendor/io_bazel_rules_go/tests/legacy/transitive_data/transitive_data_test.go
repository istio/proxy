package transitive_data

import (
	"flag"
	"os"
	"testing"
)

func TestFiles(t *testing.T) {
	filenames := flag.Args()
	if len(filenames) == 0 {
		t.Fatal("no filenames given")
	}

	for _, filename := range flag.Args() {
		if _, err := os.Stat(filename); err != nil {
			t.Error(err)
		}
	}
}
