package hello
import akka.actor.ActorSystem
import akka.http.scaladsl.Http
import akka.http.scaladsl.server.Route
import akka.stream.Materializer
import scalaz.{-\/, \/, \/-}

import scala.concurrent.{ExecutionContext, Future}
import scala.util.control.NonFatal

object Server {
  def start(host: String, port: Int, services: Route)(
      implicit ec: ExecutionContext,
      mat: Materializer,
      system: ActorSystem): Future[String \/ Http.ServerBinding] =
    Http()
      .bindAndHandle(services, host, port)
      .map(b => \/-(b))
      .recover {
        case NonFatal(t) =>
          -\/(s"Failed to initialize server due to: ${t.getMessage}")
      }
}
