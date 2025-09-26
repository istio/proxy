package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"regexp"
	"strings"

	"golang.org/x/exp/maps"
)

func main() {
	args := os.Args[1:]

	if len(args) != 6 {
		fmt.Println("Usage: expand_template <template> <out> <substitutions_json> <volatile_status_file> <stable_status_file> <executable>")
		os.Exit(1)
	}

	executable := args[5] == "true" || args[5] == "1"

	substitutions := map[string]string{}
	substitutionJson, err := os.ReadFile(args[2])
	if err != nil {
		log.Fatal(fmt.Errorf("failed to read substitutions file: %w", err))
	}
	if err := json.Unmarshal(substitutionJson, &substitutions); err != nil {
		log.Fatal(fmt.Errorf("failed to parse substitutions file: %w", err))
	}

	volatileStatus, err := parseStatusFile(args[3])
	if err != nil {
		log.Fatal(fmt.Errorf("failed to parse volatile status file: %w", err))
	}
	stableStatus, err := parseStatusFile(args[4])
	if err != nil {
		log.Fatal(fmt.Errorf("failed to parse stable status file: %w", err))
	}

	statuses := map[string]string{}
	maps.Copy(statuses, volatileStatus)
	maps.Copy(statuses, stableStatus)

	for key, value := range substitutions {
		for token, replacement := range statuses {
			value = strings.ReplaceAll(value, fmt.Sprintf("{{%s}}", token), replacement)
		}
		substitutions[key] = value
	}

	contentBytes, err := os.ReadFile(args[0])
	if err != nil {
		log.Fatal(fmt.Errorf("failed to read the template file: %w", err))
	}
	content := string(contentBytes)

	for key, value := range substitutions {
		content = strings.ReplaceAll(content, key, value)
	}

	var mode os.FileMode = 0o666
	if executable {
		mode = 0o777
	}
	err = os.WriteFile(args[1], []byte(content), mode)
	if err != nil {
		log.Fatal(fmt.Errorf("failed to write output file: %w", err))
	}
}

// captures every `KEY VALUE` line in the status file.
// for explanation see: https://regex101.com/r/cr6wX1/1
var STATUS_REGEX = regexp.MustCompile(`(?m)^([^\s]+)[[:blank:]]+([^\n]*)$`)

func parseStatusFile(statusFilePath string) (map[string]string, error) {
	statusFile, err := os.ReadFile(statusFilePath)
	if err != nil {
		return nil, err
	}

	matches := STATUS_REGEX.FindAllStringSubmatch(string(statusFile), -1)

	results := map[string]string{}

	for _, match := range matches {
		results[match[1]] = match[2]
	}

	return results, nil
}
