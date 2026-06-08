package tests.integration.plugin_targets;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Warmup;
import org.openjdk.jmh.infra.Blackhole;

@Fork(1)
@Warmup(iterations = 2, time = 1)
@Measurement(iterations = 5, time = 1)
public class ClassUsingTestOnlyAnnotationProcessor {
  @Benchmark
  public void stringBuilder(final Blackhole blackhole) {
    blackhole.consume(new StringBuilder().append("foo").append("bar").toString());
  }

  @Benchmark
  public void stringJoin(final Blackhole blackhole) {
    blackhole.consume(String.join("", "foo", "bar"));
  }
}
