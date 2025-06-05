// Copyright 2015 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;

using Grpc.Core;
using Grpc.Net.Client;

class Program
{
    /// <summary>
    /// Sample client code that makes gRPC calls to the server.
    /// </summary>
    public class Client
    {
        readonly RouteGuide.RouteGuide.RouteGuideClient client;

        public Client(RouteGuide.RouteGuide.RouteGuideClient client)
        {
            this.client = client;
        }

        /// <summary>
        /// Blocking unary call example.  Calls GetFeature and prints the response.
        /// </summary>
        public void GetFeature(int lat, int lon)
        {
            try
            {
                Log("*** GetFeature: lat={0} lon={1}", lat, lon);

                RouteGuide.Point request = new RouteGuide.Point { Latitude = lat, Longitude = lon };

                RouteGuide.Feature feature = client.GetFeature(request);
                if (feature.Exists())
                {
                    Log("Found feature called \"{0}\" at {1}, {2}",
                        feature.Name, feature.Location.GetLatitude(), feature.Location.GetLongitude());
                }
                else
                {
                    Log("Found no feature at {0}, {1}",
                        feature.Location.GetLatitude(), feature.Location.GetLongitude());
                }
            }
            catch (RpcException e)
            {
                Log("RPC failed " + e);
                throw;
            }
        }


        /// <summary>
        /// Server-streaming example. Calls listFeatures with a rectangle of interest. Prints each response feature as it arrives.
        /// </summary>
        public async Task ListFeatures(int lowLat, int lowLon, int hiLat, int hiLon)
        {
            try
            {
                Log("*** ListFeatures: lowLat={0} lowLon={1} hiLat={2} hiLon={3}", lowLat, lowLon, hiLat,
                    hiLon);

                RouteGuide.Rectangle request = new RouteGuide.Rectangle
                {
                    Lo = new RouteGuide.Point { Latitude = lowLat, Longitude = lowLon },
                    Hi = new RouteGuide.Point { Latitude = hiLat, Longitude = hiLon }
                };

                using (var call = client.ListFeatures(request))
                {
                    var responseStream = call.ResponseStream;
                    StringBuilder responseLog = new StringBuilder("Result: ");

                    while (await responseStream.MoveNext())
                    {
                        RouteGuide.Feature feature = responseStream.Current;
                        responseLog.Append(feature.ToString());
                    }
                    Log(responseLog.ToString());
                }
            }
            catch (RpcException e)
            {
                Log("RPC failed " + e);
                throw;
            }
        }

        /// <summary>
        /// Client-streaming example. Sends numPoints randomly chosen points from features
        /// with a variable delay in between. Prints the statistics when they are sent from the server.
        /// </summary>
        public async Task RecordRoute(List<RouteGuide.Feature> features, int numPoints)
        {
            try
            {
                Log("*** RecordRoute");
                using (var call = client.RecordRoute())
                {
                    // Send numPoints points randomly selected from the features list.
                    StringBuilder numMsg = new StringBuilder();
                    Random rand = new Random();
                    for (int i = 0; i < numPoints; ++i)
                    {
                        int index = rand.Next(features.Count);
                        RouteGuide.Point point = features[index].Location;
                        Log("Visiting point {0}, {1}", point.GetLatitude(), point.GetLongitude());

                        await call.RequestStream.WriteAsync(point);

                        // A bit of delay before sending the next one.
                        await Task.Delay(rand.Next(1000) + 500);
                    }
                    await call.RequestStream.CompleteAsync();

                    RouteGuide.RouteSummary summary = await call.ResponseAsync;
                    Log("Finished trip with {0} points. Passed {1} features. "
                        + "Travelled {2} meters. It took {3} seconds.", summary.PointCount,
                        summary.FeatureCount, summary.Distance, summary.ElapsedTime);

                    Log("Finished RecordRoute");
                }
            }
            catch (RpcException e)
            {
                Log("RPC failed", e);
                throw;
            }
        }

        /// <summary>
        /// Bi-directional streaming example. Send some chat messages, and print any
        /// chat messages that are sent from the server.
        /// </summary>
        public async Task RouteChat()
        {
            try
            {
                Log("*** RouteChat");
                var requests = new List<RouteGuide.RouteNote>
                {
                    NewNote("First message", 0, 0),
                    NewNote("Second message", 0, 1),
                    NewNote("Third message", 1, 0),
                    NewNote("Fourth message", 0, 0)
                };

                using (var call = client.RouteChat())
                {
                    var responseReaderTask = Task.Run(async () =>
                    {
                        while (await call.ResponseStream.MoveNext())
                        {
                            var note = call.ResponseStream.Current;
                            Log("Got message \"{0}\" at {1}, {2}", note.Message,
                                note.Location.Latitude, note.Location.Longitude);
                        }
                    });

                    foreach (RouteGuide.RouteNote request in requests)
                    {
                        Log("Sending message \"{0}\" at {1}, {2}", request.Message,
                            request.Location.Latitude, request.Location.Longitude);

                        await call.RequestStream.WriteAsync(request);
                    }
                    await call.RequestStream.CompleteAsync();
                    await responseReaderTask;

                    Log("Finished RouteChat");
                }
            }
            catch (RpcException e)
            {
                Log("RPC failed", e);
                throw;
            }
        }

        private void Log(string s, params object[] args)
        {
            Console.WriteLine(string.Format(s, args));
        }

        private void Log(string s)
        {
            Console.WriteLine(s);
        }

        private RouteGuide.RouteNote NewNote(string message, int lat, int lon)
        {
            return new RouteGuide.RouteNote
            {
                Message = message,
                Location = new RouteGuide.Point { Latitude = lat, Longitude = lon }
            };
        }
    }

    static void Main(string[] args)
    {
        Console.WriteLine("Starting Client...");

        var Port = 50051;
        var PortVar = System.Environment.GetEnvironmentVariable("SERVER_PORT");
        if (!String.IsNullOrEmpty(PortVar)) {
            Port = Int32.Parse(PortVar);
        }

        var channel = GrpcChannel.ForAddress("http://127.0.0.1:" + Port);
        var routeGuideClient = new RouteGuide.RouteGuide.RouteGuideClient(channel);
        var client = new Client(routeGuideClient);

        // Looking for a valid feature
        client.GetFeature(409146138, -746188906);

        // Feature missing.
        client.GetFeature(0, 0);

        // Looking for features between 40, -75 and 42, -73.
        client.ListFeatures(400000000, -750000000, 420000000, -730000000).Wait();

        // Record a few randomly selected points from the features file.
        client.RecordRoute(RouteGuideUtil.ParseFeatures("csharp/example/routeguide/client.exe/routeguide_features.json"), 10).Wait();

        // Send and receive some notes.
        client.RouteChat().Wait();

        channel.ShutdownAsync().Wait();
    }
}
