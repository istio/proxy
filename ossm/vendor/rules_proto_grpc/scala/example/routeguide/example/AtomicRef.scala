package example

import java.util.concurrent.atomic.AtomicReference

import scala.annotation.tailrec

class AtomicRef[T](initial: T) {
  private val ref: AtomicReference[T] = new AtomicReference[T](initial)

  def get: T = ref.get()

  @tailrec final def updateAndGet(update: T => T): T = {
    val oldValue = ref.get
    val newValue = update(oldValue)
    if (ref.compareAndSet(oldValue, newValue)) {
      newValue
    } else {
      updateAndGet(update)
    }
  }
}
