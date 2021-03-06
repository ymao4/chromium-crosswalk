// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Valid WEBP image.
var validWEBPImageCase = {
  filename: "valid.webp",
  blobString: "RIFF0\0\0\0WEBPVP8 $\0\0\0\xB2\x02\0\x9D\x01\x2A" +
              "\x01\0\x01\0\x2F\x9D\xCE\xE7s\xA8((((\x01\x9CK(\0" +
              "\x05\xCE\xB3l\0\0\xFE\xD8\x80\0\0"
}

// Write an invalid test image and expect failure.
var invalidWEBPImageCase = {
  filename: "invalid.webp",
  blobString: "abc123"
}

function runCopyToTest(testCase, expectSucceed) {
  var galleries;
  var testImageFileEntry;

  chrome.mediaGalleries.getMediaFileSystems(testGalleries);

  function testGalleries(results) {
    galleries = results;
    chrome.test.assertTrue(galleries.length > 0,
                           "Need at least one media gallery to test copyTo");

    // Create a temporary file system and an image for copying-into test.
    window.webkitRequestFileSystem(window.TEMPORARY, 1024*1024,
                                   temporaryFileSystemCallback,
                                   chrome.test.fail);
  }

  function temporaryFileSystemCallback(filesystem) {
    filesystem.root.getFile(testCase.filename, {create:true, exclusive: false},
                            temporaryImageCallback,
                            chrome.test.fail);
  }

  function temporaryImageCallback(entry) {
    testImageFileEntry = entry;
    entry.createWriter(imageWriterCallback, chrome.test.fail);
  }

  function imageWriterCallback(writer) {
    // Go through a Uint8Array to avoid UTF-8 control bytes.
    var blobBytes = new Uint8Array(testCase.blobString.length);
    for (var i = 0; i < testCase.blobString.length; i++) {
      blobBytes[i] = testCase.blobString.charCodeAt(i);
    }
    var blob = new Blob([blobBytes], {type : "image/webp"});

    writer.onerror = function(e) {
      chrome.test.fail("Unable to write test image: " + e.toString());
    }

    writer.onwriteend = testCopyTo(testImageFileEntry, galleries[0],
                                   testCase.filename, expectSucceed);

    writer.write(blob);
  }

  function testCopyTo(testImageFileEntry, gallery, filename, expectSucceed) {
    var onSuccess;
    var onFailure;
    if (expectSucceed) {
      onSuccess = chrome.test.succeed;
      onFailure = chrome.test.fail;
    } else {
      onSuccess = chrome.test.fail;
      onFailure = chrome.test.succeed;
    }
    return function() {
      testImageFileEntry.copyTo(gallery.root, filename, onSuccess, onFailure);
    };
  }
}

// The custom callback functions are options.
// If they are undefined, the default callbacks and checks will be used.
// If passed in, all 3 function arguments need to be valid.
function runReadGalleriesTest(expectedGalleryCount, expectSucceed,
                              customReadDirectoryCallback,
                              customReadDirectoryErrorCallback,
                              customGotGalleriesCallback) {
  var galleries;
  var readEntriesResults = [];
  var readDirectoryCallback;
  var readDirectoryErrorCallback;
  var gotGalleriesCallback;

  if (customReadDirectoryCallback && customReadDirectoryErrorCallback &&
      customGotGalleriesCallback) {
    chrome.test.assertEq(typeof(customReadDirectoryCallback), 'function');
    chrome.test.assertEq(typeof(customReadDirectoryErrorCallback), 'function');
    chrome.test.assertEq(typeof(customGotGalleriesCallback), 'function');
    readDirectoryCallback = customReadDirectoryCallback;
    readDirectoryErrorCallback = customReadDirectoryErrorCallback;
    gotGalleriesCallback = customGotGalleriesCallback;
  } else {
    chrome.test.assertTrue(!customReadDirectoryCallback &&
                           !customReadDirectoryErrorCallback &&
                           !customGotGalleriesCallback);
    readDirectoryCallback = defaultReadDirectoryCallback;
    readDirectoryErrorCallback = defaultReadDirectoryErrorCallback;
    gotGalleriesCallback = defaultGotGalleriesCallback;
  }
  chrome.mediaGalleries.getMediaFileSystems(testGalleries);

  function testGalleries(results) {
    gotGalleriesCallback(results);
    chrome.test.assertEq(expectedGalleryCount, results.length,
                         "Gallery count mismatch");
    if (expectedGalleryCount == 0) {
      chrome.test.succeed();
      return;
    }

    for (var i = 0; i < results.length; i++) {
      var dirReader = results[i].root.createReader();
      dirReader.readEntries(readDirectoryCallback, readDirectoryErrorCallback);
    }
  }

  function defaultGotGalleriesCallback(entries) {
    galleries = entries;
  }

  function defaultReadDirectoryCallback(entries) {
    var result = "";
    if (!expectSucceed) {
      result = "Unexpected readEntries success";
    }
    readEntriesResults.push(result);
    checkReadEntriesFinished();
  }

  function defaultReadDirectoryErrorCallback(err) {
    var result = "";
    if (expectSucceed) {
      result = "Unexpected readEntries failure: " + err;
    }
    readEntriesResults.push(result);
    checkReadEntriesFinished();
  }

  function checkReadEntriesFinished() {
    if (readEntriesResults.length != galleries.length)
      return;
    var success = true;
    for (var i = 0; i < readEntriesResults.length; i++) {
      if (readEntriesResults[i]) {
        success = false;
      }
    }
    if (success) {
      chrome.test.succeed();
      return;
    }
    chrome.test.fail(readEntriesResults);
  }
}

// Create a dummy window to prevent the ExtensionProcessManager from suspending
// the chrome-test app. Needed because the writer.onerror and writer.onwriteend
// events do not qualify as pending callbacks, so the app looks dormant.
function CreateDummyWindowToPreventSleep() {
  chrome.app.runtime.onLaunched.addListener(function() {
    chrome.app.window.create('dummy.html', {
      bounds: {
        width: 800,
        height: 600,
        left: 100,
        top: 100
      },
      minWidth: 800,
      minHeight: 600
    });
  });
}
