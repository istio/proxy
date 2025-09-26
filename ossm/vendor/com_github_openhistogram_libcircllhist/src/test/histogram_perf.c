#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <circllhist.h>
#include <sys/time.h>
#include <assert.h>

typedef histogram_t *(*halloc_func)();

halloc_func halloc = NULL;

histogram_t *build(histogram_t *out, double *vals, int nvals) {
  int i;
  if(out == NULL) out = halloc();
  for(i=0;i<nvals;i++)
    hist_insert(out, vals[i], 1);
  return out;
}
double *buildNvals(int n) {
  int i;
  double *vals = malloc(sizeof(*vals) * n);
  for(i=0;i<n;i++) {
    vals[i] = (0.1 + (double)(i % 10)) * pow(10,(i/10));
  }
  return vals;
}

struct sval {
  int64_t val;
  int scale;
};
histogram_t *buildI(histogram_t *out, struct sval *vals, int nvals) {
  int i;
  if(out == NULL) out = halloc();
  for(i=0;i<nvals;i++)
    hist_insert_raw(out, int_scale_to_hist_bucket(vals[i].val, vals[i].scale), 1);
  return out;
}
struct sval *buildNIvals(int n) {
  int i;
  struct sval *vals = malloc(sizeof(*vals) * n);
  for(i=0;i<n;i++) {
    vals[i].val = ((i%90)+10);
    vals[i].scale = i/90;
  }
  return vals;
}

const int iters[] = { 100, 10000, 100000 };
const int sizes[] = { 31, 127, 255 };
int main() {
  int i, s;
  for(i=0;i<sizeof(iters)/sizeof(*iters);i++) {
    for(s=0;s<sizeof(sizes)/sizeof(*sizes);s++) {
      struct timeval start, finish;
      histogram_t *hist = NULL;
      int idx, ai;
      int iter = iters[i];
      int size = sizes[s];
      long cnt = 0;
      for(ai=0;ai<2;ai++) {
        halloc = (ai%2 == 0) ? hist_alloc: hist_fast_alloc;

{ // double
      cnt = 0;
      hist = NULL;
      printf("%s,%d,%d,%d,",
             (ai%2 == 0) ? "normal" : "fast",
             iter, (size*iter), size);
      double *vals = buildNvals(size);
      hist = build(hist, vals, size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = build(hist, vals, size);
        cnt += size;
      }
      assert(hist_num_buckets(hist) == size);
      gettimeofday(&finish, NULL);
      double elapsed = finish.tv_sec - start.tv_sec;
      elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
      if(cnt != 0)
        printf("%0.2f\n",
               (elapsed / (double)cnt) * 1000000000.0);
      else 
        printf("cannot calculate benchmark, no work done!\n");
      hist_free(hist);
      free(vals);
}
{ // int
      hist = NULL;
      cnt = 0;
      printf("%s,%d,%d,%d,",
             (ai%2 == 0) ? "normal" : "fast",
             iter, (size*iter), size);
      struct sval *vals = buildNIvals(size);
      hist = buildI(hist, vals, size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = buildI(hist, vals, size);
        cnt += size;
      }
      assert(hist_num_buckets(hist) == size);
      gettimeofday(&finish, NULL);
      double elapsed = finish.tv_sec - start.tv_sec;
      elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
      if(cnt != 0)
        printf("%0.2f\n",
               (elapsed / (double)cnt) * 1000000000.0);
      else
        printf("cannot calculate benchmark, no work done!\n");
      hist_free(hist);
      free(vals);
}
      }
    }
  }
}
