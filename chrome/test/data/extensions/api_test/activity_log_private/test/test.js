// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The extension ID for the .../activity_log_private/friend extension, which
// this extension communicates with.  This should correspond to the public key
// defined in .../activity_log_private/friend/manifest.json.
var FRIEND_EXTENSION_ID = 'pknkgggnfecklokoggaggchhaebkajji';

// Setup the test cases.
var testCases = [];
testCases.push({
  func: function triggerApiCall() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'api_call', function response() { });
  },
  expected_activity: ['cookies.set']
});
testCases.push({
  func: function triggerSpecialCall() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'special_call', function response() { });
  },
  expected_activity: [
    'extension.getURL',
    'extension.getViews'
  ]
});
testCases.push({
  func: function triggerDouble() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'double', function response() {});
  },
  expected_activity: ['omnibox.setDefaultSuggestion']
});
testCases.push({
  func: function triggerAppBindings() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'app_bindings', function response() { });
  },
  expected_activity: [
    'app.GetDetails',
    'app.GetIsInstalled',
    'app.getInstallState'
  ]
});
testCases.push({
  func: function triggerObjectMethods() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'object_methods', function response() { });
  },
  expected_activity: ['storage.clear']
});
testCases.push({
  func: function triggerMessageSelf() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'message_self', function response() { });
  },
  expected_activity: [
    'runtime.connect',
    'runtime.sendMessage'
  ]
});
testCases.push({
  func: function triggerMessageOther() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'message_other', function response() { });
  },
  expected_activity: [
    'runtime.connect',
    'runtime.sendMessage'
  ]
});
testCases.push({
  func: function triggerConnectOther() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'connect_other', function response() { });
  },
  expected_activity: ['runtime.connect']
});
testCases.push({
  func: function triggerBackgroundXHR() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'background_xhr', function response() { });
  },
  expected_activity: [
    'XMLHttpRequest.open',
    'XMLHttpRequest.setRequestHeader'
  ]
});
testCases.push({
  func: function triggerTabIds() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'tab_ids', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.move',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerTabIdsIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'tab_ids_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.move',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerWebRequest() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'webrequest', function response() { });
  },
  expected_activity: [
    'webRequestInternal.addEventListener',
    'webRequestInternal.addEventListener',
    'webRequest.onBeforeSendHeaders/1',
    'webRequestInternal.eventHandled',
    'webRequest.onBeforeSendHeaders',
    'webRequest.onHeadersReceived/2',
    'webRequestInternal.eventHandled',
    'webRequest.onHeadersReceived',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ],
});

testCases.push({
  func: function triggerWebRequestIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'webrequest_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'webRequestInternal.addEventListener',
    'webRequestInternal.addEventListener',
    'windows.create',
    'webRequest.onBeforeSendHeaders/3',
    'webRequestInternal.eventHandled',
    'webRequest.onBeforeSendHeaders',
    'webRequest.onHeadersReceived/4',
    'webRequestInternal.eventHandled',
    'webRequest.onHeadersReceived',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ],
});

testCases.push({
  func: function triggerApiCallsOnTabsUpdated() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'api_tab_updated', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.connect',
    'tabs.sendMessage',
    'tabs.executeScript',
    'tabs.executeScript',
    'HTMLDocument.write',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerApiCallsOnTabsUpdatedIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'api_tab_updated_incognito',
                               function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.connect',
    'tabs.sendMessage',
    'tabs.executeScript',
    'tabs.executeScript',
    'HTMLDocument.write',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerFullscreen() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'launch_dom_fullscreen',
                               function response() { });
  },
  expected_activity: [
    'extension.getURL',
    'test.getConfig',
    'Element.webkitRequestFullscreen'
  ]
});

var domExpectedActivity = [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
     // Location access
    'Window.location',
    'Document.location',
    'Window.location',
    'Location.assign',
    'Location.replace',
     // Dom mutations
    'Document.createElement',
    'Document.createElement',
    'Node.appendChild',
    'Node.insertBefore',
    'Node.replaceChild',
    //'Document.location',
    'HTMLDocument.write',
    'HTMLDocument.writeln',
    'HTMLElement.innerHTML',
    // Navigator access
    'Window.navigator',
    'Geolocation.getCurrentPosition',
    'Geolocation.watchPosition',
    // Web store access - session storage
    'Window.sessionStorage',
    'Storage.setItem',
    'Storage.getItem',
    'Storage.removeItem',
    'Storage.clear',
    // Web store access - local storage
    'Window.localStorage',
    'Storage.setItem',
    'Storage.getItem',
    'Storage.removeItem',
    'Storage.clear',
    // Notification access
    'Window.webkitNotifications',
    'NotificationCenter.createNotification',
    // Cache access
    'Window.applicationCache',
    // Web database access
    'Window.openDatabase',
    // Canvas access
    'Document.createElement',
    'HTMLCanvasElement.getContext',
    // XHR from content script.
    'XMLHttpRequest.open',
    'XMLHttpRequest.setRequestHeader',
    'HTMLDocument.write'
];

// add the hook activity
hookNames = ['onclick', 'ondblclick', 'ondrag', 'ondragend', 'ondragenter',
             'ondragleave', 'ondragover', 'ondragstart', 'ondrop', 'oninput',
             'onkeydown', 'onkeypress', 'onkeyup', 'onmousedown',
             'onmouseenter', 'onmouseleave', 'onmousemove', 'onmouseout',
             'onmouseover', 'onmouseup', 'onmousewheel'];

for (var i = 0; i < hookNames.length; i++) {
  domExpectedActivity.push('HTMLElement.' + hookNames[i]);
  domExpectedActivity.push('Document.' + hookNames[i]);
  domExpectedActivity.push('Window.' + hookNames[i]);
}

// Close the tab.
domExpectedActivity.push('tabs.remove');

testCases.push({
  func: function triggerDOMChangesOnTabsUpdated() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'dom_tab_updated', function response() { });
  },
  expected_activity: domExpectedActivity
});

testCases.push({
  func: function triggerDOMChangesOnTabsUpdatedIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'dom_tab_updated_incognito',
                               function response() { });
  },
  is_incognito: true,
  expected_activity: ['windows.create'].concat(domExpectedActivity)
});

testCases.push({
  func: function checkSavedHistory() {
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = 'tabs.onUpdated';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.assertEq('tabs.onUpdated',
              result['activities'][0]['apiCall']);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function checkHistoryForURL() {
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.pageUrl = 'http://www.google.com';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function checkOtherObject() {
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'dom_access';
    filter.apiCall = 'Document.location';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.assertEq('Document.location',
              result['activities'][0]['apiCall']);
          chrome.test.assertEq('setter',
              result['activities'][0]['other']['domVerb']);
          chrome.test.succeed();
        });
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = 'webRequest.onHeadersReceived';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.assertEq('webRequest.onHeadersReceived',
              result['activities'][0]['apiCall']);
          chrome.test.assertEq('{"added_request_headers":true}',
              result['activities'][0]['other']['webRequest']);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function deleteGoogleUrls() {
    chrome.test.getConfig(function(config) {
      chrome.activityLogPrivate.deleteUrls(
          ['http://www.google.com:' + config.testServer.port]);

      var filter = new Object();
      filter.extensionId = FRIEND_EXTENSION_ID;
      filter.activityType = 'any';
      filter.pageUrl = 'http://www.google.com';
      chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(0, result['activities'].length);
          chrome.test.succeed();
        });
     });
  }
});

testCases.push({
  func: function deleteAllUrls() {
    chrome.activityLogPrivate.deleteUrls([]);
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.pageUrl = 'http://';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(0, result['activities'].length);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function deleteAllHistory() {
    chrome.activityLogPrivate.deleteDatabase();
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = '';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(0, result['activities'].length);
          chrome.test.succeed();
        });
  }
});

function checkIncognito(url, incognitoExpected) {
  if (url) {
    incognitoExpected = Boolean(incognitoExpected);
    var kIncognitoMarker = '<incognito>';
    var isIncognito =
        (url.substr(0, kIncognitoMarker.length) == kIncognitoMarker);
    chrome.test.assertEq(incognitoExpected, isIncognito,
                         'Bad incognito state for URL ' + url);
  }
}

// Listener to check the expected logging is done in the test cases.
var testCaseIndx = 0;
var callIndx = -1;
var enabledTestCases = [];

chrome.activityLogPrivate.onExtensionActivity.addListener(
    function(activity) {
      var activityId = activity['extensionId'];
      chrome.test.assertEq(FRIEND_EXTENSION_ID, activityId);

      // Check the api call is the one we expected next.
      var apiCall = activity['apiCall'];
      expectedCall = 'runtime.onMessageExternal';
      var testCase = enabledTestCases[testCaseIndx];
      if (callIndx > -1) {
        expectedCall = testCase.expected_activity[callIndx];
      }
      console.log('Logged:' + apiCall + ' Expected:' + expectedCall);
      chrome.test.assertEq(expectedCall, apiCall);

      // Check that no real URLs are logged in incognito-mode tests.  Ignore
      // the initial call to windows.create opening the tab.
      if (apiCall != 'windows.create') {
        checkIncognito(activity['pageUrl'], testCase.is_incognito);
        checkIncognito(activity['argUrl'], testCase.is_incognito);
      }

      // If all the expected calls have been logged for this test case then
      // mark as suceeded and move to the next. Otherwise look for the next
      // expected api call.
      if (callIndx == testCase.expected_activity.length - 1) {
        chrome.test.succeed();
        callIndx = -1;
        testCaseIndx++;
      } else {
        callIndx++;
      }
    }
);

function setupTestCasesAndRun() {
  chrome.runtime.getPlatformInfo(function(info) {
    var tests = [];
    for (var i = 0; i < testCases.length; i++) {
      // Ignore test case if disabled for this OS.
      if (testCases[i].disabled != undefined &&
          info.os in testCases[i].disabled &&
          testCases[i].disabled[info.os]) {
        console.log('Test case disabled for this OS: ' + info.os);
        continue;
      }

      // Add the test case to the enabled list and set the expected activity
      // appriorate for this OS.
      if (testCases[i].func != undefined) {
        tests.push(testCases[i].func);
        var enabledTestCase = testCases[i];
        var activityListForOS = 'expected_activity_' + info.os;
        if (activityListForOS in enabledTestCase) {
          console.log('Expecting OS specific activity for: ' + info.os);
          enabledTestCase.expected_activity =
              enabledTestCase[activityListForOS];
        }
        enabledTestCases.push(enabledTestCase);
      }
    }
    chrome.test.runTests(tests);
  });
}

setupTestCasesAndRun();
