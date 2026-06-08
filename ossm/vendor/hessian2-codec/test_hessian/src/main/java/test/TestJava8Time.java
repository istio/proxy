/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package test;

import java.time.*;

public class TestJava8Time {

    public static Duration java8_Duration() {
        return Duration.ofSeconds(30, 10);
    }

    public static Instant java8_Instant() {
        return Instant.ofEpochSecond(100, 10);
    }

    public static LocalDate java8_LocalDate() {
        return LocalDate.of(2020, 6, 16);
    }

    public static LocalDateTime java8_LocalDateTime() {
        return LocalDateTime.of(2020, 6, 16, 6, 5, 4, 3);
    }

    public static LocalTime java8_LocalTime() {
        return LocalTime.of(6, 16);
    }

    public static MonthDay java8_MonthDay() {
        return MonthDay.of(6, 16);
    }

    public static OffsetDateTime java8_OffsetDateTime() {
        return OffsetDateTime.of(2020, 6, 16, 6, 5, 4, 3, java8_ZoneOffset());
    }

    public static OffsetTime java8_OffsetTime() {
        return OffsetTime.of(6, 5, 4, 3, java8_ZoneOffset());
    }

    public static Period java8_Period() {
        return Period.of(2020, 6, 16);
    }

    public static Year java8_Year() {
        return Year.of(2020);
    }

    public static YearMonth java8_YearMonth() {
        return YearMonth.of(2020, 6);
    }

    public static ZonedDateTime java8_ZonedDateTime() {
        ZonedDateTime of = ZonedDateTime.of(java8_LocalDateTime(), java8_ZoneId());
        return of;
    }

    public static ZoneId java8_ZoneId() {
        return ZoneId.of("Z");
    }

    public static ZoneOffset java8_ZoneOffset() {
        return ZoneOffset.ofHours(2);
    }

}
