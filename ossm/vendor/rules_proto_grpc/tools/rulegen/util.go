package main

import (
	"bytes"
	"crypto/sha256"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"
	"text/template"
)

type LineWriter struct {
	lines []string
}

func (w *LineWriter) w(s string, args ...interface{}) {
	w.lines = append(w.lines, fmt.Sprintf(s, args...))
}

func (w *LineWriter) t(t *template.Template, data interface{}, indent string) {
	var buf bytes.Buffer
	err := t.Execute(&buf, data)
	if err != nil {
		log.Fatalf("%v", err)
	}
	w.lines = append(w.lines, indent + strings.ReplaceAll(buf.String(), "\n", "\n" + indent))
}

func (w *LineWriter) tpl(filename string, data interface{}) {
	tpl, err := template.ParseFiles(filename)
	if err != nil {
		log.Fatalf("Failed to parse %s: %v", filename, err)
	}
	w.t(tpl, data, "")
}

func (w *LineWriter) ln() {
	w.lines = append(w.lines, "")
}

func (w *LineWriter) MustWrite(filepath string) {
	err := ioutil.WriteFile(filepath, []byte(strings.Join(w.lines, "\n")), 0666)
	if err != nil {
		log.Fatalf("FAIL %s: %v", filepath, err)
	}
	log.Printf("Wrote %s", filepath)
}

func mustTemplate(tpl string) *template.Template {
	return template.Must(template.New("").Option("missingkey=error").Parse(tpl))
}

func mustGetSha256(url string) string {
	response, err := http.Get(url)
	if err != nil {
		log.Fatal(err)
	}
	defer response.Body.Close()

	h := sha256.New()
	if _, err = io.Copy(h, response.Body); err != nil {
		log.Fatal(err)
	}

	sha256sum := fmt.Sprintf("%x", h.Sum(nil))

	log.Printf("sha256 for %s is %q", url, sha256sum)

	return sha256sum
}

func stringInSlice(search string, slice []string) bool {
	for _, item := range slice {
		if item == search {
			return true
		}
	}
	return false
}

func doTestOnPlatform(lang *Language, rule *Rule, ciPlatform string) bool {
	// Load platforms to skip
	var skipTestPlatforms []string
	if rule != nil && len(rule.SkipTestPlatforms) > 0 {
		skipTestPlatforms = rule.SkipTestPlatforms
	} else {
		skipTestPlatforms = lang.SkipTestPlatforms
	}

	// Check for special none token
	if stringInSlice("none", skipTestPlatforms) {
		return true
	}

	// Check for special all token
	if stringInSlice("all", skipTestPlatforms) {
		return false
	}

	// Convert to CI platforms
	for platform, checkCiPlatforms := range ciPlatformsMap {
		if stringInSlice(platform, skipTestPlatforms) {
			for _, checkCiPlatform := range checkCiPlatforms {
				skipTestPlatforms = append(skipTestPlatforms, checkCiPlatform)
			}
		}
	}

	// Check for platform
	if stringInSlice(ciPlatform, skipTestPlatforms) {
		return false
	}

	return true
}

func fileExists(name string) bool {
    _, err := os.Stat(name)
    return !os.IsNotExist(err)
}
