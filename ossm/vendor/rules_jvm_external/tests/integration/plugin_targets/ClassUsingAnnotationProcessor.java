import com.google.auto.value.AutoValue;

@AutoValue
abstract class ClassUsingAnnotationProcessor {
  static ClassUsingAnnotationProcessor create(String field) {
    return new AutoValue_ClassUsingAnnotationProcessor(field);
  }

  abstract String field();
}
