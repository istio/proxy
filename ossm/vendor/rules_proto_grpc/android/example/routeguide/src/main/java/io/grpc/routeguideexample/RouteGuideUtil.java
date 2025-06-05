/*
 * Copyright 2015 The gRPC Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.grpc.routeguideexample;

import io.grpc.examples.routeguide.Feature;
import io.grpc.examples.routeguide.Point;


/** Common utilities for the RouteGuide demo. */
public class RouteGuideUtil {
  private static final double COORD_FACTOR = 1e7;

  /** Gets the latitude for the given point. */
  public static double getLatitude(Point location) {
    return location.getLatitude() / COORD_FACTOR;
  }

  /** Gets the longitude for the given point. */
  public static double getLongitude(Point location) {
    return location.getLongitude() / COORD_FACTOR;
  }

  /** Indicates whether the given feature exists (i.e. has a valid name). */
  public static boolean exists(Feature feature) {
    return feature != null && !feature.getName().isEmpty();
  }
}
