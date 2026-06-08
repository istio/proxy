package hello

import akka.actor.ActorSystem
import akka.stream.ActorMaterializer
import com.typesafe.scalalogging.LazyLogging
import scalaz.EitherT.{eitherT, fromDisjunction => pure}
import scalaz.Scalaz._
import scalaz.\/

import scala.concurrent.{ExecutionContext, Future}

object Application extends App with LazyLogging {
  implicit val system: ActorSystem = ActorSystem("sample")
  implicit val materializer: ActorMaterializer = ActorMaterializer()
  implicit val executionContext: ExecutionContext = system.dispatcher

  val run: () => Future[String \/ Unit] = { () =>
    (for {
      c <- pure[Future](Configuration.parse)
      s <- eitherT(Server.start(c.host, c.port, Endpoint.route))
    } yield {
      logger.info(s"Started server on host = ${c.host} and port = ${c.port}")
    }).run
  }

  run()
}
