/**
 * Copied from https://github.com/btlines/grpcexample/blob/master/src/main/scala/io/grpc/routeguide/RouteGuideClient.scala
 * License: MIT
 */
package example

import java.io.IOException
import java.util.concurrent.{CountDownLatch, TimeUnit}
import java.util.logging.Logger

import io.grpc.{ManagedChannelBuilder, Status}
import io.grpc.stub.StreamObserver

import io.grpc.examples.routeguide.routeguide.{Feature, Point, RouteNote, RouteSummary, Rectangle, RouteGuideGrpc}

import scala.io.StdIn
import scala.util.{Random, Try, Success, Failure}

class RouteGuideClient(host: String, port: Int) {

  val logger: Logger = Logger.getLogger(classOf[RouteGuideClient].getName)

  val channel =
    ManagedChannelBuilder
      .forAddress(host, port)
      .usePlaintext()
      .build()

  val blockingStub = RouteGuideGrpc.blockingStub(channel)
  val asyncStub = RouteGuideGrpc.stub(channel)

  def shutdown(): Unit = channel.shutdown().awaitTermination(5, TimeUnit.SECONDS)

  import io.grpc.StatusRuntimeException

  /**
    * Blocking unary call example.  Calls getFeature and prints the response.
    */
  def getFeature(lat: Int, lon: Int): Unit = {
    logger.info(s"*** GetFeature: lat=$lat lon=$lon")
    val request = Point(lat, lon)
    try {
      val feature = blockingStub.getFeature(request)
      val lat = RouteGuideService.getLatitude(feature.getLocation)
      val lon =  RouteGuideService.getLongitude(feature.getLocation)
      if (RouteGuideService.isValid(feature)) {
        logger.info(s"Found feature called '${feature.name}' at $lat, $lon")
      } else {
        logger.info(s"Found no feature at $lat, $lon")
      }
    } catch {
      case e: StatusRuntimeException =>
        logger.warning(s"RPC failed:${e.getStatus}")
        sys.exit(1)
    }
  }

  import io.grpc.StatusRuntimeException

  /**
    * Blocking server-streaming example. Calls listFeatures with a rectangle of interest. Prints each
    * response feature as it arrives.
    */
  def listFeatures(lowLat: Int, lowLon: Int, hiLat: Int, hiLon: Int): Unit = {
    logger.info(s"*** ListFeatures: lowLat=$lowLat lowLon=$lowLon hiLat=$hiLat hiLon=$hiLon")
    val request = Rectangle(
      lo = Some(Point(lowLat, lowLon)),
      hi = Some(Point(hiLat, hiLon))
    )
    try {
      val features = blockingStub.listFeatures(request)
      features.zipWithIndex.foreach { case (feature, index) =>
        logger.info(s"Result #$index: $feature")
      }
    } catch {
      case e: StatusRuntimeException =>
        logger.warning(s"RPC failed: ${e.getStatus}")
        sys.exit(1)
    }
  }

  /**
    * Async client-streaming example. Sends {@code numPoints} randomly chosen points from {@code
    * features} with a variable delay in between. Prints the statistics when they are sent from the
    * server.
    */
  @throws[InterruptedException]
  def recordRoute(features: Seq[Feature], numPoints: Int): Unit = {
    logger.info("*** RecordRoute features=" + features.size)
    val finishLatch = new CountDownLatch(1)
    val responseObserver = new StreamObserver[RouteSummary]() {
      override def onNext(summary: RouteSummary): Unit = {
        logger.info(s"Finished trip with ${summary.pointCount} points. Passed ${summary.featureCount} features. " + s"Travelled ${summary.distance} meters. It took ${summary.elapsedTime} seconds.")
      }

      override def onError(t: Throwable): Unit = {
        logger.warning(s"RecordRoute Failed: ${Status.fromThrowable(t)}")
        finishLatch.countDown()
        sys.exit(1)
      }

      override def onCompleted(): Unit = {
        logger.info("Finished RecordRoute")
        finishLatch.countDown()
      }
    }
    val requestObserver = asyncStub.recordRoute(responseObserver)
    try { // Send numPoints points randomly selected from the features list.
      (0 to numPoints).foreach { i =>
        if (finishLatch.getCount > 0) {
          val index = Random.nextInt(features.size)
          val point = features(index).getLocation
          logger.info(s"Visiting point ${point.latitude}, ${point.longitude}")
          requestObserver.onNext(point)
          // Sleep for a bit before sending the next one.
          Thread.sleep(Random.nextInt(1000) + 500)
        }
      }
    } catch {
      case e: RuntimeException =>
        // Cancel RPC
        requestObserver.onError(e)
        throw e
    }
    // Mark the end of requests
    requestObserver.onCompleted()
    // Receiving happens asynchronously
    if (!finishLatch.await(1, TimeUnit.MINUTES)) logger.warning("recordRoute can not finish within 1 minutes")
  }

  /**
    * Bi-directional example, which can only be asynchronous. Send some chat messages, and print any
    * chat messages that are sent from the server.
    */
  def routeChat: CountDownLatch = {
    logger.info("*** RouteChat")
    val finishLatch = new CountDownLatch(1)
    val requestObserver = asyncStub.routeChat(new StreamObserver[RouteNote]() {
      override def onNext(note: RouteNote): Unit = {
        logger.info(s"Got message '${note.message}' at ${note.getLocation.latitude}, ${note.getLocation.longitude}")
      }

      override def onError(t: Throwable): Unit = {
        logger.warning(s"RouteChat Failed: ${Status.fromThrowable(t)}")
        finishLatch.countDown()
        sys.exit(1)
      }

      override def onCompleted(): Unit = {
        logger.info("Finished RouteChat")
        finishLatch.countDown()
      }
    })
    try {
      val requests = Seq(
        RouteNote(message = "First message", location = Some(Point(0, 0))),
        RouteNote(message = "Second message", location = Some(Point(0, 1))),
        RouteNote(message = "Third message", location = Some(Point(1, 0))),
        RouteNote(message = "Fourth message", location = Some(Point(1, 1)))
      )
      for (request <- requests) {
        logger.info(s"Sending message '${request.message}' at ${request.getLocation.latitude}, ${request.getLocation.longitude}")
        requestObserver.onNext(request)
      }
    } catch {
      case e: RuntimeException =>
        // Cancel RPC
        requestObserver.onError(e)
        throw e
    }
    // Mark the end of requests
    requestObserver.onCompleted()
    // return the latch while receiving happens asynchronously
    finishLatch
  }

}

object RouteGuideClient extends App {
  val logger = Logger.getLogger(getClass.getName)

  val features = RouteGuidePersistence.getFeatures

  val port = if (System.getenv("SERVER_PORT") != null) {
    System.getenv("SERVER_PORT").toInt
  } else 50051

  val client = new RouteGuideClient("localhost", port)
  var stop = false

  try {
    client.getFeature(409146138, -746188906)
    client.listFeatures(400000000, -750000000, 420000000, -730000000)
    client.recordRoute(features, 2)
    val finishLatch = client.routeChat
    if (!finishLatch.await(1, TimeUnit.MINUTES)) logger.warning("routeChat can not finish within 1 minutes")
  } finally client.shutdown()

}
