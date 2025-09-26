package hello

import akka.http.scaladsl.model.{HttpResponse, StatusCodes}
import akka.http.scaladsl.server.Directives._
import akka.http.scaladsl.server.Route

object Endpoint {
  val route: Route =
    get {
      pathSingleSlash {
        complete(HttpResponse(StatusCodes.OK))
      }
    }
}
