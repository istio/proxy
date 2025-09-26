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

public class TestString {

    public static String getEmojiTestString() {
        int[] emoji_ucs4 = new int[] { 0x0001_f923 };
        String s = new String(emoji_ucs4, 0, emoji_ucs4.length);

        s += ",max";

        // see: http://www.unicode.org/glossary/#code_point
        int[] max_unicode_ucs4 = new int[] { 0x0010_ffff };
        s += new String(max_unicode_ucs4, 0, max_unicode_ucs4.length);

        return s;
    }

    public static String getComplexString() {
        String s = "킐\u0088中国你好!\u0088\u0088\u0088\u0088\u0088\u0088";
        return s;
    }

    public static String getSuperComplexString() {
        String s = "킐\u0088中国你好!\u0088\u0088\u0088\u0088\u0088\u0088✅❓☑️😊🤔👀🫅🔒🗝️🧫🛹🚅🧻🪞🪞🪞🪞🪞🪞🪞🪞🪞🕟🕟🕟🕟🕟🕟🕟🔅🔅🔅🔅🔅🔅🤍🤍🤍🤍🤍🤍🌈🌈🌈🌈🌈🌈🏦🏦🏦🏦🏦🏦🚎🚎🚎🚎🚎🚎🚎⏰⏰⏰⏰⏰⏲️⏲️⏲️🗄️abcdefghijklmnopqrstuvwxyz1234567@#$%^&*()_+⏲️⏲️⏲️⏲️🐪🐫c⏰";
        return s;
    }
}
