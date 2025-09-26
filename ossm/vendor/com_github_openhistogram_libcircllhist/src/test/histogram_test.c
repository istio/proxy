#if defined(linux) || defined(__linux) || defined(__linux__)
#  define _XOPEN_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "circllhist.h"
#include <string.h>
#include <inttypes.h>
#include <time.h>

typedef histogram_t *(*halloc_func)();
halloc_func halloc = NULL;

static int tcount = 1;
static int failed = 0;
static char *test_desc = "??";
#define okf(fmt, ex...) do { \
  printf("ok %d - %s : " fmt "\n", tcount++, test_desc, ex); \
} while(0)
#define ok() do { \
  printf("ok %d - %s\n", tcount++, test_desc); \
} while(0)
#define notokf(fmt, ex...) do { \
  printf("not ok %d - %s : " fmt "\n", tcount++, test_desc, ex); \
  failed++; \
} while(0)
#define notok() do { \
  printf("not ok %d - %s\n", tcount++, test_desc); \
  failed++; \
} while(0)
static void is(int expr) {
  if(expr) { ok(); } else { notok(); }
};
#define isf(expr, fmt, ex...) do { \
  if(expr) { \
    printf("ok %d - %s : " fmt "\n", tcount++, test_desc, ex); \
  } else { \
    printf("not ok %d - %s : " fmt "\n", tcount++, test_desc, ex); \
    failed++; \
  } \
} while(0)
#define T(a) do { \
  test_desc = #a; \
  a; \
  test_desc = "??"; \
} while(0)

bool double_equals(double a, double b) {
  double r, diff, max = fabs(a);
  if(fabs(b) > max) max = fabs(b);
  if(max == 0) return true;
  diff = b-a;
  r = diff/max;
  if(fabs(r) < 0.0001) return true;
  return false;
}

void bucket_tests() {
  hist_bucket_t b, o;
  char hbstr[HIST_BUCKET_MAX_STRING_SIZE] = {0};

  b = int_scale_to_hist_bucket(INT64_MIN, 1);
  T(is(b.val == -92 && b.exp == 19));
  b = int_scale_to_hist_bucket(INT64_MAX, 1);
  T(is(b.val == 92 && b.exp == 19));
  b = int_scale_to_hist_bucket(INT64_MIN, -127);
  T(is(b.val == -92 && b.exp == -109));
  b = int_scale_to_hist_bucket(INT64_MAX, -200);
  T(is(b.val == 0 && b.exp == 0));
  b = int_scale_to_hist_bucket(10, -128);
  T(is(b.val == 10 && b.exp == -127));

  b = int_scale_to_hist_bucket(0,0);
  T(is(b.val == 0 && b.exp == 0));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "0")==0));

  b = int_scale_to_hist_bucket(100,0);
  T(is(b.val == 10 && b.exp == 2));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e+001")==0));

  b = double_to_hist_bucket(100.0);
  T(is(b.val == 10 && b.exp == 2));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e+001")==0));

  b = int_scale_to_hist_bucket(2,0);
  o = double_to_hist_bucket(2);
  T(is(b.val == o.val && b.exp == o.exp));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+20e-001")==0));

  b = int_scale_to_hist_bucket(1,-9);
  o = double_to_hist_bucket(1e-9);
  T(is(b.val == o.val && b.exp == o.exp));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e-010")==0));

  b = int_scale_to_hist_bucket(1300000000,-9);
  o = double_to_hist_bucket(1.3);
  T(is(b.val == o.val && b.exp == o.exp));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+13e-001")==0));

  b = int_scale_to_hist_bucket(-2700,-9);
  o = double_to_hist_bucket(-2.7e-6);
  T(is(b.val == o.val && b.exp == o.exp));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-27e-007")==0));

  b = int_scale_to_hist_bucket(7,-9);
  o = double_to_hist_bucket(7e-9);
  T(is(b.val == o.val && b.exp == o.exp));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+70e-010")==0));

  b = double_to_hist_bucket(0);
  T(is(b.val == 0 && b.exp == 0));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "0")==0));

  b = double_to_hist_bucket(9.9999e-129);
  T(is(b.val == 0 && b.exp == 0));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "0")==0));

  b = double_to_hist_bucket(1e-128);
  T(is(b.val == 10 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e-129")==0));

  b = double_to_hist_bucket(1.00001e-128);
  T(is(b.val == 10 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e-129")==0));

  b = double_to_hist_bucket(1.09999e-128);
  T(is(b.val == 10 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e-129")==0));

  b = double_to_hist_bucket(1.1e-128);
  T(is(b.val == 11 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+11e-129")==0));

  b = double_to_hist_bucket(1e127);
  T(is(b.val == 10 && b.exp == 127));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+10e+126")==0));

  b = double_to_hist_bucket(9.999e127);
  T(is(b.val == 99 && b.exp == 127));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+99e+126")==0));

  b = double_to_hist_bucket(1e128);
  T(is(b.val == -1 && b.exp == 0));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "NaN")==0));

  // negative range

  b = double_to_hist_bucket(-9.9999e-129);
  T(is(b.val == 0 && b.exp == 0));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "0")==0));

  b = double_to_hist_bucket(-1e-128);
  T(is(b.val == -10 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-10e-129")==0));

  b = double_to_hist_bucket(-1.00001e-128);
  T(is(b.val == -10 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-10e-129")==0));

  b = double_to_hist_bucket(-1.09999e-128);
  T(is(b.val == -10 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-10e-129")==0));

  b = double_to_hist_bucket(-1.1e-128);
  T(is(b.val == -11 && b.exp == -128));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-11e-129")==0));

  b = double_to_hist_bucket(-1e127);
  T(is(b.val == -10 && b.exp == 127));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-10e+126")==0));

  b = double_to_hist_bucket(-9.999e127);
  T(is(b.val == -99 && b.exp == 127));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "-99e+126")==0));

  b = double_to_hist_bucket(-1e128);
  T(is(b.val == -1 && b.exp == 0));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "NaN")==0));

  b = double_to_hist_bucket(9.999e127);
  T(is(b.val == 99 && b.exp == 127));
  hist_bucket_to_string(b, hbstr);
  T(is(strcmp(hbstr, "+99e+126")==0));
}

void test1(double val, double b, double w) {
  double out, interval;
  hist_bucket_t in;
  in = double_to_hist_bucket(val);
  out = hist_bucket_to_double(in);
  interval = hist_bucket_to_double_bin_width(in);
  if(out < 0) interval *= -1.0;
  if(double_equals(b,out)) ok();
  else notokf("(%f bin %g != %g)\n", val, out, b);
  if(double_equals(w,interval)) ok();
  else notokf("(%f width %f != %f)\n", val, interval, w);
}

histogram_t *build(double *vals, int nvals) {
  int i;
  histogram_t *out = halloc();
  for(i=0;i<nvals;i++)
    hist_insert(out, vals[i], 1);
  return out;
}
void mean_test(double *vals, int nvals, double expected) {
  histogram_t *h = build(vals, nvals);
  double m = hist_approx_mean(h);
  if(double_equals(m,expected)) ok();
  else notokf("(mean() -> %g != %g)\n", m, expected);
  hist_free(h);
}
void q_test(double *vals, int nvals, double *in, int nin, double *expected) {
  double *out;
  histogram_t *h = build(vals, nvals);
  out = calloc(nin, sizeof(*out));
  int rv = hist_approx_quantile(h, in, nin, out);
  if(rv != 0) notokf("quantile -> %d", rv);
  else {
    int i;
    for(i=0;i<nin;i++) {
      if(!double_equals(out[i], expected[i])) {
        notokf("q(%f) -> %g != %g", in[i], out[i], expected[i]);
        free(out);
        return;
      }
    }
    ok();
  }
  hist_free(h);
  free(out);
}

void simple_clear() {
  histogram_t *h = hist_alloc();
  double out[1], in[1] = {0};
  hist_insert_intscale(h, 1, 0, 1);
  hist_approx_quantile(h, in, 1, out);
  if(out[0] != 1) notokf("preclear q(0) -> %g != 1", out[0]);
  else ok();
  hist_clear(h);
  out[0] = 0;
  hist_approx_quantile(h, in, 1, out);
  if(!isnan(out[0])) notokf("postclear q(0) -> %g != NaN", out[0]);
  else ok();
  hist_free(h);
}

void accum_sub_test() {
  int i, j, samples = 0;
  histogram_t *tgt;
  histogram_t *t[10] = {NULL};
  for(i=0;i<10;i++) {
    if(i==8) continue;
    t[i] = hist_alloc();
    for(j=0; j<100; j++) {
      hist_insert(t[i], (lrand48() % 100) + 10, 1);
      samples++;
    }
  }
  tgt = hist_alloc();
  hist_accumulate(tgt, (const histogram_t * const*)t, 10);
  isf(samples == hist_sample_count(tgt), "should have %d samples", samples);
  int rv = hist_subtract(tgt, (const histogram_t * const*)t, 9);
  if(rv < 0) notokf("hist_subtract underrun: %d", rv);
  else ok();
  if(hist_sample_count(tgt) != hist_sample_count(t[9]))
    notokf("subtract resulted in wrong sample count: %" PRIu64 " != %" PRIu64 "",
            hist_sample_count(tgt), hist_sample_count(t[9]));
  else ok();
}

void serialize_test() {
  int i,j,lfailed=0;
  histogram_t *in, *out;

  /* We build it clear it and build it one shorter.. This way the 0.13 bucket will be zero */
  double s[] = { 0.123, 0, 0.43, 0.41, 0.415, 0.2201, 0.3201, 0.125, 0.13, 13};
  in = build(s, 10);
  uint64_t sc;
  sc = hist_sample_count(in);
  isf(sc == 10, "%s: %lu", "sample count", (unsigned long)sc);
  hist_clear(in);
  sc = hist_sample_count(in);
  isf(sc == 0, "%s: %lu", "sample count after clear", (unsigned long)sc);
  for(i=0;i<9;i++) hist_insert(in, s[i], 1);
  sc = hist_sample_count(in);
  isf(sc == 9, "%s: %lu", "sample count after reinsertion", (unsigned long)sc);

  ssize_t len = hist_serialize_estimate(in);
  char *serial;
  serial = malloc(len);
  len = hist_serialize(in, serial, len);
  out = halloc();
  hist_deserialize(out, serial, len);

  if(hist_bucket_count(out) <= hist_bucket_count(in))
    okf("%s", "serialized equal or smaller");
  else notokf("%s", "serialized bigger");
  if(hist_bucket_count(out) == 0) notokf("%s", "serialized to zero");
  for(j=0, i=0; i<hist_bucket_count(in); i++) {
    hist_bucket_t ib, ob;
    uint64_t ic, oc;
    hist_bucket_idx_bucket(in, i, &ib, &ic);
    if(ic == 0) continue;
    hist_bucket_idx_bucket(out, j++, &ob, &oc);
    if(!(ib.val == ob.val && ib.exp == ob.exp && ic == oc))
      lfailed = 1;
  }
  if(lfailed) notokf("%s", "histograms match");
  else okf("%s %d buckets (%d empty)", "histograms match", j, i-j);

  hist_free(in);
  hist_free(out);
  free(serial);
}

void sample_count_roll() {
  histogram_t *toobig;
  toobig = hist_alloc();
  hist_insert(toobig, 1, ~((uint64_t)0));
  hist_insert(toobig, 2, ~((uint64_t)0));
  is(hist_sample_count(toobig) == ~((uint64_t)0));
  hist_free(toobig);
}

void compress_test() {
  double s[] = { 0,1,2,3,10,11,12,21,22,23,99,100,110,120,210,220 };
  // mbe = 0:    0 1 2 3 10 11 12 21 22 23 90 100 110 120 210 220 => 16 buckets
  // mbe = 1:    0 0 0 0 10 10 10 20 20 20 90 100 110 120 210 220 => 9 buckets
  // mbe = 2:    0 0 0 0 0  0  0  0  0  0  0  100 100 100 200 200 => 3 buckets
  histogram_t *tofree;
  histogram_t *h = build(s, sizeof(s)/sizeof(double));
  tofree = h; 
  h = hist_compress_mbe(h, 0);
  hist_free(tofree);
  T(is(hist_bucket_count(h) == 16));
  tofree = h;
  h = hist_compress_mbe(h, 1);
  hist_free(tofree);
  T(is(hist_bucket_count(h) == 9));
  tofree = h;
  h = hist_compress_mbe(h, 2);
  hist_free(tofree);
  T(is(hist_bucket_count(h) == 3));
  tofree = h;
  h = hist_compress_mbe(h, 3);
  hist_free(tofree);
  T(is(hist_bucket_count(h) == 1));
  
  hist_free(h);
}

void clone_test() {
  double s[] = { 0,1,2,3,10,11,12,21,22,23,99,100,110,120,210,220 };
  int i, j, lfailed = 0;
  // mbe = 0:    0 1 2 3 10 11 12 21 22 23 90 100 110 120 210 220 => 16 buckets
  // mbe = 1:    0 0 0 0 10 10 10 20 20 20 90 100 110 120 210 220 => 9 buckets
  // mbe = 2:    0 0 0 0 0  0  0  0  0  0  0  100 100 100 200 200 => 3 buckets
  histogram_t *h = build(s, sizeof(s)/sizeof(double));
  histogram_t *clone = hist_clone(h);

  if(hist_bucket_count(clone) <= hist_bucket_count(h))
    okf("%s", "clone equal or smaller");
  else notokf("%s", "clone bigger");
  if(hist_bucket_count(clone) == 0) notokf("%s", "cloned to zero");
  for(j=0, i=0; i<hist_bucket_count(h); i++) {
    hist_bucket_t ib, ob;
    uint64_t ic, oc;
    hist_bucket_idx_bucket(h, i, &ib, &ic);
    if(ic == 0) continue;
    hist_bucket_idx_bucket(clone, j++, &ob, &oc);
    if(!(ib.val == ob.val && ib.exp == ob.exp && ic == oc))
      lfailed = 1;
  }
  if(lfailed) notokf("%s", "histograms match");
  else okf("%s %d buckets (%d empty)", "histograms match", j, i-j);

  hist_free(h);
  hist_free(clone);
}


static int my_free_called = 0;
void my_free(void *x) {
  my_free_called = 1;
  free(x);
}

static int my_calloc_called = 0;
void* my_calloc(size_t n, size_t x) {
  my_calloc_called = 1;
  return calloc(n, x);
}

void allocator_test() {
  double s[] = { 0,1,2,3,10,11,12,21,22,23,99,100,110,120,210,220 };
  int i, j, lfailed = 0;
  // mbe = 0:    0 1 2 3 10 11 12 21 22 23 90 100 110 120 210 220 => 16 buckets
  // mbe = 1:    0 0 0 0 10 10 10 20 20 20 90 100 110 120 210 220 => 9 buckets
  // mbe = 2:    0 0 0 0 0  0  0  0  0  0  0  100 100 100 200 200 => 3 buckets

  // replace `free` and `calloc` as a test
  hist_allocator_t my_allocator = {
    .calloc = my_calloc,
    .malloc = malloc,
    .free = my_free
  };

  histogram_t *h = build(s, sizeof(s)/sizeof(double));
  histogram_t *clone = hist_clone_with_allocator(h, &my_allocator);

  if(hist_bucket_count(clone) <= hist_bucket_count(h))
    okf("%s", "clone equal or smaller");
  else notokf("%s", "clone bigger");
  if(hist_bucket_count(clone) == 0) notokf("%s", "cloned to zero");
  for(j=0, i=0; i<hist_bucket_count(h); i++) {
    hist_bucket_t ib, ob;
    uint64_t ic, oc;
    hist_bucket_idx_bucket(h, i, &ib, &ic);
    if(ic == 0) continue;
    hist_bucket_idx_bucket(clone, j++, &ob, &oc);
    if(!(ib.val == ob.val && ib.exp == ob.exp && ic == oc))
      lfailed = 1;
  }
  if(lfailed) notokf("%s", "histograms match");
  else okf("%s %d buckets (%d empty)", "histograms match", j, i-j);

  hist_free(h);
  hist_free(clone);
  if (my_free_called == 0) {
    notokf("%s", "my_free not called");
  } else {
    okf("%s", "my_free called");
  }

  if (my_calloc_called == 0) {
    notokf("%s", "my_calloc not called");
  } else {
    okf("%s", "my_calloc called");
  }
}

static void issue_n() {
  histogram_t* main_thread_interval_hist = hist_alloc();
  histogram_t* per_thread_interval_hist = hist_alloc();

  hist_insert_intscale(per_thread_interval_hist, 1, 0, 1);

  double in[9] = {0, 0.25, 0.5, 0.75, 0.90, 0.95, 0.99, 0.999, 1};
  double out[9];
  hist_approx_quantile(main_thread_interval_hist, in, 9,out);

  const histogram_t* const hist_array[1] = { per_thread_interval_hist };
  hist_accumulate(main_thread_interval_hist, hist_array, 1);
  hist_clear(per_thread_interval_hist);

  hist_insert_intscale(per_thread_interval_hist, 2, 0, 1);
  hist_approx_quantile(per_thread_interval_hist, in, 9,out);
  isf(out[0] == 2, " min==2.0 != %g", out[0]);

  main_thread_interval_hist = hist_alloc();
  hist_accumulate(main_thread_interval_hist, hist_array, 1);
  hist_approx_quantile(main_thread_interval_hist, in, 9,out);
  isf(out[0] == 2, "min==1.0 != %g", 2.0);

  histogram_t* direct_hist = hist_alloc();
  hist_insert_intscale(direct_hist, 2, 0, 1);
  hist_approx_quantile(direct_hist, in, 9,out);
  isf(out[0] == 2, " min==2.0 != %g", out[0]);
}

void
iq_test() {
  int size = 10;
  double in[10] = {-200,-100,0,1,1.001,1.1,1.2,2,3,4};
  double out[10];
  memset(&out, 0, sizeof(out));
  int rc;
  // NULL hist: no inverse quantiles
  rc = hist_approx_inverse_quantile(NULL, in, size, out);
  is(rc == 0);
  for (int i=0;i<size;i++) {
    if(isnan(out[i])) { ok(); }
  }
  // empty hist: no inverse quantiles
  double a1[] = {};
  memset(&out, 0, sizeof(out));
  histogram_t *h1 = build(a1, sizeof(a1)/sizeof(double));
  rc = hist_approx_inverse_quantile(h1, in, size, out);
  T(is(rc == 0));
  for (int i = 0; i < size; i++) {
    if(isnan(out[i])) { ok(); }
  }
  // negative hist
  memset(&out, 0, sizeof(out));
  double a2[] = { -100 };
  histogram_t *h2 = build(a2, sizeof(a2)/sizeof(double));
  rc = hist_approx_inverse_quantile(h2, in, size, out);
  T(is(rc == 0));
  T(is(out[0] == 0));
  T(is(out[1] == 1));
  T(is(out[2] == 1));
  // zero hist
  memset(&out, 0, sizeof(out));
  double a3[] = { 0 };
  histogram_t *h3 = build(a3, sizeof(a3)/sizeof(double));
  rc = hist_approx_inverse_quantile(h3, in, size, out);
  T(is(rc == 0));
  T(is(out[1] == 0));
  T(is(out[2] == 0));
  T(is(out[3] == 1));
  // normal hist
  memset(&out, 0, sizeof(out));
  double a4[] = { 1,2,3 };
  histogram_t *h4 = build(a4, sizeof(a4)/sizeof(double));
  rc = hist_approx_inverse_quantile(h4, in, size, out);
  T(is(rc == 0));
  T(is(double_equals(out[4], 1./300)));
  T(is(double_equals(out[5], 1./3)));
  T(is(double_equals(out[6], 1./3)));
  T(is(double_equals(out[7], 1./3)));
  T(is(double_equals(out[8], 2./3)));
  T(is(double_equals(out[9], 1)));
}

int main() {
  srand48(time(NULL));
  bucket_tests();
  T(test1(43.3, 43, 1));
  T(test1(99.9, 99, 1));
  T(test1(10, 10, 1));
  T(test1(1, 1, 0.1));
  T(test1(0.0002, 0.0002, 0.00001));
  T(test1(0.003, 0.003, 0.0001));
  T(test1(0.3201, 0.32, 0.01));
  T(test1(0.0035, 0.0035, 0.0001));
  T(test1(-1, -1, -0.1));
  T(test1(-0.00123, -0.0012, -0.0001));
  T(test1(-987324, -980000, -10000));

  halloc = hist_alloc;

  iq_test();
  
  for(int ai=0; ai<2; ai++) {
    double s1[] = { 0.123, 0, 0.43, 0.41, 0.415, 0.2201, 0.3201, 0.125, 0.13 };
    T(mean_test(s1, 9, 0.24444));

    double h[] = { 1 };
    double qin[] = { 0, 0.25, 0.5, 1 };
    double qout[] = { 1, 1.025, 1.05, 1.1 };
    T(q_test(h, 1, qin, 4, qout));

    double qin2[] = { 0, 0.95, 0.99, 1.0 };
    double qout2[] = { 0, 0.4355, 0.4391, 0.44 };
    T(q_test(s1, 9, qin2, 4, qout2));

    double s3[] = { 1.0, 2.0 };
    double qin3[] = { 0.5 };
    double qout3[] = { 1.1 };
    T(q_test(s3, 2, qin3, 1, qout3));

    double s4[] = { 1.0, 1e200 }; // out of range -> nan bucket
    double qin4[] = { 0, 1 };
    double qout4[] = { 1.0, 1.1 };
    T(q_test(s4, 2, qin4, 2, qout4));
    T(mean_test(s4, 2, 1.05));

    double s5[] = { 1e200, 1e200, 1e200,  0, 0, 1e-20, 1e-20, 1e-20, 1e-10};
    double qin5[] = { 0, 1 };
    double qout5[] = { 0, 1.1e-10 };
    T(q_test(s5, 9, qin5, 2, qout5));

    double s6[] = { 0, 1 };
    double qin6[] = { 0, 0.1 };
    double qout6[] = { 0, 0 };
    T(q_test(s6, 2, qin6, 2, qout6));

    T(serialize_test());

    T(clone_test());

    T(allocator_test());

    halloc = hist_fast_alloc;
  }

  T(sample_count_roll());

  T(accum_sub_test());
  compress_test();

  T(simple_clear());

  T(issue_n());

  T(is(isnan(hist_approx_mean(NULL))));
  T(is(isnan(hist_approx_stddev(NULL))));
  T(is(isnan(hist_approx_moment(NULL, 1))));
  T(is(0 == hist_approx_sum(NULL)));
  T(is(0 == hist_approx_count_below(NULL, 1)));
  T(is(0 == hist_approx_count_above(NULL, 1)));

  printf("%d..%d\n", 1, tcount-1);
  return failed ? -1 : 0;
}
