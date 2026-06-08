package hello
import akka.http.scaladsl.model.StatusCodes
import akka.http.scaladsl.testkit.ScalatestRouteTest
import org.scalatest.wordspec.AnyWordSpec
import org.scalatest.matchers.should.Matchers

class EndpointSpec extends AnyWordSpec with Matchers with ScalatestRouteTest {
  Endpoint.getClass.getSimpleName should {
    "should return status OK" in {
      Get() ~> Endpoint.route ~> check {
        status shouldEqual StatusCodes.OK
      }
    }
  }
}
