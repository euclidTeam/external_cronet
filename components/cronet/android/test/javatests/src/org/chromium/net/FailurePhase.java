/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.chromium.net;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        FailurePhase.START, FailurePhase.READ_SYNC, FailurePhase.READ_ASYNC,
        FailurePhase.MAX_FAILURE_PHASE
})
@Retention(RetentionPolicy.SOURCE)
public @interface FailurePhase {
    int START = 0;
    int READ_SYNC = 1;
    int READ_ASYNC = 2;
    int MAX_FAILURE_PHASE = 3;
}