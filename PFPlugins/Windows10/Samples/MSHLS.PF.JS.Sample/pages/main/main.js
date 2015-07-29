// For an introduction to the Page Control template, see the following documentation:
// http://go.microsoft.com/fwlink/?LinkId=232511
(function () {
    "use strict";

    function onResize(e) {
        var player = document.getElementById("player");
        var width = document.body.clientWidth;
        var height = document.body.clientHeight;

        player.winControl.width = width;
        player.winControl.height = height - 40;
    }

    WinJS.UI.Pages.define("/pages/main/main.html", {
        // This function is called whenever a user navigates to this page. It
        // populates the page elements with the app's data.
        ready: function (element, options) {
            var player = document.getElementById("player");
            var width = WinJS.Utilities.getTotalWidth(element);
            var height = WinJS.Utilities.getTotalHeight(element);
            player.winControl.width = width;
            player.winControl.height = height - 40;

            window.addEventListener("resize", onResize, false);
        },

        unload: function () {
            // TODO: Respond to navigations away from this page.
        },

        updateLayout: function (element) {
            /// <param name="element" domElement="true" />

            // TODO: Respond to changes in layout.
        }
    });
})();
