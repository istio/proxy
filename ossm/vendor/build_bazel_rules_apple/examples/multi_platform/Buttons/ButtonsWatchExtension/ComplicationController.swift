// Copyright 2018 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import ClockKit

class ComplicationController: NSObject, CLKComplicationDataSource {

  // MARK: - Timeline Configuration

  func getSupportedTimeTravelDirections(for complication: CLKComplication,
                                        withHandler handler: @escaping (CLKComplicationTimeTravelDirections) -> Void) {
    handler([.forward, .backward])
  }

  func getTimelineStartDate(for complication: CLKComplication,
                            withHandler handler: @escaping (Date?) -> Void) {
    handler(nil)
  }

  func getTimelineEndDate(for complication: CLKComplication,
                          withHandler handler: @escaping (Date?) -> Void) {
    handler(nil)
  }

  func getPrivacyBehavior(for complication: CLKComplication,
                          withHandler handler: @escaping (CLKComplicationPrivacyBehavior) -> Void) {
    handler(.showOnLockScreen)
  }

  // MARK: - Timeline Population

  func getCurrentTimelineEntry(for complication: CLKComplication,
                               withHandler handler: @escaping (CLKComplicationTimelineEntry?) -> Void) {
    handler(nil)
  }

  func getTimelineEntries(for complication: CLKComplication,
                          before date: Date,
                          limit: Int,
                          withHandler handler: @escaping ([CLKComplicationTimelineEntry]?) -> Void) {
    handler(nil)
  }

  func getTimelineEntries(for complication: CLKComplication,
                          after date: Date,
                          limit: Int,
                          withHandler handler: @escaping ([CLKComplicationTimelineEntry]?) -> Void) {
    handler(nil)
  }

  // MARK: - Placeholder Templates

  func getLocalizableSampleTemplate(for complication: CLKComplication,
                                    withHandler handler: @escaping (CLKComplicationTemplate?) -> Void) {
    handler(nil)
  }

}
