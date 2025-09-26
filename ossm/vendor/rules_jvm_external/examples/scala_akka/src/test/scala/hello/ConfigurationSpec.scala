package hello

import org.scalatest.Inside
import org.scalatest.wordspec.AnyWordSpec
import org.scalatest.matchers.should.Matchers
import scalaz.\/-

class ConfigurationSpec extends AnyWordSpec with Matchers with Inside {
  Configuration.getClass.getSimpleName should {
    "should parse reference configuration" in {
      val sut = Configuration.parse
      inside(sut) {
        case \/-(c) =>
          c.host shouldBe "0.0.0.0"
          c.port shouldBe 9000
      }
    }
  }
}
