package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"sync"

	"github.com/google/go-containerregistry/pkg/registry"
	"github.com/r3labs/diff/v3"
)

type Authn struct {
	Authorization []string `json:"Authorization"`
}

func main() {

	var auth Authn

	scanner := bufio.NewScanner(os.Stdin)

	go func() {
		for scanner.Scan() {
			content := scanner.Text()
			auth = Authn{}
			err := json.Unmarshal([]byte(content), &auth)
			if err != nil {
				log.Fatalln(err)
			}
			fmt.Println(content)
		}
		if scanner.Err() != nil {
			log.Fatalln(scanner.Err())
		}
	}()

	reg := registry.New(
		registry.Logger(log.New(io.Discard, "", log.LstdFlags)),
	)

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fmt.Println("Request URI:", r.RequestURI)
		if strings.Contains(r.RequestURI, "/token") {
			if r.Header["Authorization"] == nil || r.Header["Authorization"][0] != "Basic dGVzdDp0ZXN0" {
				fmt.Println("Unauthorized request", r.Header["Authorization"])
				w.WriteHeader(http.StatusUnauthorized)
				w.Write([]byte("{\"errors\":[{\"code\":\"UNAUTHORIZED\",\"message\":\"authentication required\"}]}"))
			} else {
				w.WriteHeader(http.StatusOK)
				w.Write([]byte("{\"token\": \"here_is_the_token\"}"))
			}
			return
		} else if strings.Contains(r.RequestURI, "/v2/empty_image/") {
			currentAuth := Authn{Authorization: []string{}}
			if r.Header["Authorization"] != nil {
				currentAuth.Authorization = r.Header["Authorization"]
			}

			var ret []byte

			changes, err := diff.Diff(currentAuth, auth)

			if err != nil {
				ret = []byte(err.Error())
			} else if len(changes) > 0 {
				ret, err = json.Marshal(changes)
				if err != nil {
					ret = []byte(err.Error())
				}
			}
			if len(ret) > 0 {
				fmt.Println(string(ret))
				w.WriteHeader(http.StatusUnauthorized)
				w.Write(ret)
				return
			}

		}

		reg.ServeHTTP(w, r)
	})

	insecure := &http.Server{
		Handler: handler,
		Addr:    "localhost:1448",
	}
	secure := &http.Server{
		Handler: handler,
		Addr:    "localhost:1447",
	}

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		fmt.Println("Starting insecure server on port 1448")
		err := insecure.ListenAndServe()
		if err != nil {
			fmt.Println("Error starting secure server:", err)
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		fmt.Println("Starting secure server on port 1447")
		err := secure.ListenAndServeTLS(os.Getenv("CERT"), os.Getenv("KEY"))
		if err != nil {
			fmt.Println("Error starting secure server:", err)
		}
	}()

	wg.Wait()
}
