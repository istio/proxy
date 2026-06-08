/*
 * Copyright (c) 2016-2021, Circonus, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include "circllhist.h"

bool cumulative = false;

void help(const char *prog) {
  printf("%s:\n", prog);
  printf("\t-h\t\thelp\n");
  printf("\t-a <val>\tcompute number of samples above <val>\n");
  printf("\t-b <val>\tcompute number of samples below <val>\n");
  printf("\t-p <0-100>\tcompute approximate percentile\n");
  printf("\t-i <val>\tcompute approximate inverse quantile at <val>\n");
  printf("\t-C\t\tcalculate difference between cumulative histograms\n");
  printf("\t[hist1 [hist2 [...]]]\n\n");
  printf("If no hists are specified, stdin is read\n");
  printf("\n\nExample:\n\n");
  printf("\tcurl url |\n\t  jq -r .stat._value |\n\t  %s -i 0.2 -p 0 -p 100 -p 99 -p 99.999 -p 50\n\n", prog);
  printf("\tThis will fetch the document at URL, extract the b64 encoded histogram\n");
  printf("\tprinting the inverse quantile at 0.2 (seconds)\n");
  printf("\tand the p0, p50, p99, p99.999, and p100\n");
}

void print_hist(const histogram_t *hist) {
  printf("{");
  int cnt = hist_bucket_count(hist);
  for(int i=0; i<cnt; i++) {
    double v;
    uint64_t vc;
    hist_bucket_idx(hist, i, &v, &vc);
    printf("%s\"%g\":%" PRIu64, i ? "," : "", v, vc);
  }
  printf("}\n");
}

histogram_t *decode(const char *buff) {
  histogram_t *hist = hist_alloc();
  if(hist_deserialize_b64(hist, buff, strlen(buff)) <= -1) {
    fprintf(stderr, "histogram invalid\n");
    hist_free(hist);
    return NULL;
  }
  return hist;
}

histogram_t *calc_cum(const histogram_t *base, const histogram_t *now, bool cumulative) {
  histogram_t *result = hist_clone(now);
  if(cumulative && !base) return NULL;
  if(!cumulative) return result;
  if(hist_subtract(result, &base, 1) == 0) return result;
  fprintf(stderr, "histogram cumulative calculation reset\n");
  hist_free(result);
  return NULL;
}

struct calcs {
  int cnt;
  double *elements;
} above = { 0 }, below = { 0 }, quantiles = { 0 }, invquantiles = { 0 };

void add_to(struct calcs *c, double v) {
  c->elements = realloc(c->elements, sizeof(double) * (1 + c->cnt));
  c->elements[c->cnt++] = v;
}

void print(const histogram_t *hist) {
  int i;
  if(above.cnt || below.cnt || quantiles.cnt) {
    printf("{");
    for(i=0; i<above.cnt; i++) {
      uint64_t cnt = hist_approx_count_above(hist, above.elements[i]);
      printf("\"above(%g)\":%zu,", above.elements[i], (size_t)cnt);
    }
    for(i=0; i<below.cnt; i++) {
      uint64_t cnt = hist_approx_count_below(hist, below.elements[i]);
      printf("\"below(%g)\":%zu,", below.elements[i], (size_t)cnt);
    }
    double vals[quantiles.cnt];
    hist_approx_quantile(hist, quantiles.elements, quantiles.cnt, vals);
    for(i=0; i<quantiles.cnt; i++) {
      printf("\"p(%f%%)\":%g,", quantiles.elements[i] * 100, vals[i]);
    }
    double qvals[invquantiles.cnt];
    hist_approx_inverse_quantile(hist, invquantiles.elements, invquantiles.cnt, qvals);
    for(i=0; i<invquantiles.cnt; i++) {
      printf("\"invq(%f)\":%g,", invquantiles.elements[i], qvals[i]);
    }
    printf("\"count\":%zu}\n", hist_sample_count(hist));
  }
  else print_hist(hist);
}

static int double_compare(const void *lv, const void *rv) {
  double l = *(double *)lv;
  double r = *(double *)rv;
  if(l < r) return -1;
  if(l > r) return 1;
  return 0;
}

int main(int argc, char **argv) {
  double percent;
  char buff[256*1024];
  int opt;
  while((opt = getopt(argc, argv, "ha:b:p:i:C")) != -1) {
    switch(opt) {
    case 'a':
      add_to(&above, atof(optarg));
      break;
    case 'b':
      add_to(&below, atof(optarg));
      break;
    case 'i':
      add_to(&invquantiles, atof(optarg));
      break;
    case 'p':
      percent = atof(optarg);
      if(percent < 0 || percent > 100) {
        fprintf(stderr, "Invalid percentile %f\n", percent);
        exit(-1);
      }
      add_to(&quantiles, percent/100);
      break;
    case 'C':
      cumulative = true;
      break;
    case 'h':
      help(argv[0]);
      exit(0);
    default:
      fprintf(stderr, "unrecognized option: -%c\n", opt);
      exit(-1);
      break;
    }
  }

  if(quantiles.cnt) qsort(quantiles.elements, quantiles.cnt, sizeof(double), double_compare);

  histogram_t *last = NULL;
  if(optind < argc) {
    for(int i=optind; i<argc; i++) {
      if(strlen(argv[i]) > sizeof(buff)-1) {
        fprintf(stderr, "histogram too large\n");
        exit(-1);
      }
      histogram_t *hist = decode(argv[i]);
      if(hist) {
        histogram_t *toprint = calc_cum(last, hist, cumulative);
        if(toprint) print(toprint);
        if(last) hist_free(last);
        last = hist;
      }
    }
  } else {
    while(NULL != fgets(buff, sizeof(buff), stdin)) {
      histogram_t *hist = decode(buff);
      if(hist) {
        histogram_t *toprint = calc_cum(last, hist, cumulative);
        if(toprint) print(toprint);
        if(last) hist_free(last);
        last = hist;
      }
    }
  }
  if(last) hist_free(last);
  return 0;
}
