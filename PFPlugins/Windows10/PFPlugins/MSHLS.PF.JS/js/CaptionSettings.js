/*global Windows, WinJS*/

(function (PlayerFramework, undefined) {
    "use strict";

    var captionSettings = null;

    function getFont(fontStyle) {
        switch (fontStyle) {
            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.default:
                return "";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.smallCapitals:
                return "Segoe UI";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.proportionalWithSerifs:
                return "Cambria";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.proportionalWithoutSerifs:
                return "Segoe UI";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.monospacedWithSerifs:
                return "Courier New";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.monospacedWithoutSerifs:
                return "Consolas";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.cursive:
                return "Segoe Script";

            case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.casual:
                return "Segoe Print";
        }
    }

    function getSizePercent(fontSize) {
        switch (fontSize) {
            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.default:
                return 1;

            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.fiftyPercent:
                return .5;

            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.oneHundredPercent:
                return 1;

            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.oneHundredFiftyPercent:
                return 1.5;

            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.twoHundredPercent:
                return 2;
        }
    }

    function getOpacityPercent(opacity) {
        switch (opacity) {
            case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.default:
                return 1;

            case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.twentyFivePercent:
                return .25;

            case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.oneHundredPercent:
                return 1;

            case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.seventyFivePercent:
                return .75;

            case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.zeroPercent:
                return 0;
        }
    }

    function getTextShadow(fontEffect) {
        switch (fontEffect) {
            case Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.default:
                return null;

            case Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.depressed:
                return "-1px -1px 0px black";

            case Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.dropShadow:
                return "2px 2px 1px black";

            case Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.raised:
                return "1px 1px 0px black";

            case Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.uniform:
                return "-1px -1px 0 #000, 1px -1px 0 #000, -1px 1px 0 #000, 1px 1px 0 #000";

            case Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.none:
                return "";
        }
    }

    function getFontVariant(fontFamily) {
        if (fontFamily === Windows.Media.ClosedCaptioning.ClosedCaptionStyle.smallcaps) {
            return "small-caps";
        }

        return "";
    }

    function getColor(color, computedColor, opacity, defaultColor) {
        var components = defaultColor.split("(")[1].split(")")[0].split(",");
        var r = new Number(components[0]);
        var g = new Number(components[1]);
        var b = new Number(components[2]);
        var a = components.length >= 4 ? new Number(components[3]) : 1;
        
        if (color === Windows.Media.ClosedCaptioning.ClosedCaptionColor.default) {
            if (opacity === Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.default) {
                return "rgba(" + r + ", " + g + ", " + b + ", " + a + ")";
            }
            else {
                return "rgba(" + r + ", " + g + ", " + b + ", " + getOpacityPercent(opacity) + ")";
            }
        }
        else {
            if (opacity === Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.default) {
                return "rgba(" + computedColor.r + ", " + computedColor.g + ", " + computedColor.b + ", " + a + ")";
            }
            else {
                return "rgba(" + computedColor.r + ", " + computedColor.g + ", " + computedColor.b + ", " + getOpacityPercent(opacity) + ")";
            }
        }
    }

    function captionsUpdated(captionsDOM) {
        if (captionSettings) {
            var rowsContainer = captionsDOM.children[0].children[0],
                rows = rowsContainer.children,
                i,
                row,
                rowText,
                j,
                rowItem,
                fontPx,
                fontSize;

            rowsContainer.style.lineHeight = "";

            for (i = 0; i < rows.length; i = i + 1) {
                row = rows[i];

                rowText = row.innerText;

                if (rowText === "") {
                    if (row.style.height !== "10%") {
                        switch (captionSettings.fontSize) {
                            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.default:
                                break;

                            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.fiftyPercent:
                                row.style.height = "6%";
                                break;

                            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.oneHundredPercent:
                                break;

                            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.oneHundredFiftyPercent:
                                row.style.height = "5.1";
                                break;

                            case Windows.Media.ClosedCaptioning.ClosedCaptionSize.twoHundredPercent:
                                row.style.height = "5%";
                                break;
                        }
                    }
                } else {
                    row.style.height = "";
                    if (captionSettings.regionColor !== Windows.Media.ClosedCaptioning.ClosedCaptionColor.default || captionSettings.regionOpacity !== Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.default) {
                        var currentStyle = window.getComputedStyle(row);
                        row.style.backgroundColor = getColor(captionSettings.regionColor, captionSettings.computedRegionColor, captionSettings.regionOpacity, currentStyle.backgroundColor);
                    }

                    for (j = 0; j < row.children.length; j = j + 1) {
                        rowItem = row.children[j];
                        var currentStyle = window.getComputedStyle(rowItem);

                        if (captionSettings.backgroundColor !== Windows.Media.ClosedCaptioning.ClosedCaptionColor.default || captionSettings.backgroundOpacity !== Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.default) {
                            rowItem.style.backgroundColor = getColor(captionSettings.backgroundColor, captionSettings.computedBackgroundColor, captionSettings.backgroundOpacity, currentStyle.backgroundColor);
                        }

                        if (captionSettings.fontColor !== Windows.Media.ClosedCaptioning.ClosedCaptionColor.default || captionSettings.fontOpacity !== Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.default) {
                            rowItem.style.color = getColor(captionSettings.fontColor, captionSettings.computedFontColor, captionSettings.fontOpacity, currentStyle.backgroundColor);
                        }

                        if (captionSettings.fontStyle !== Windows.Media.ClosedCaptioning.ClosedCaptionStyle.default) {
                            rowItem.style.fontFamily = getFont(captionSettings.fontStyle);
                            rowItem.style.fontVariant = getFontVariant(captionSettings.fontStyle);
                        }

                        if (captionSettings.fontSize !== Windows.Media.ClosedCaptioning.default) {
                            fontPx = new Number(currentStyle.fontSize.substr(0, currentStyle.fontSize.length - 2));
                            fontSize = fontPx * getSizePercent(captionSettings.fontSize);
                            rowItem.style.fontSize = fontSize + "px";
                        }

                        if (captionSettings.fontEffect !== Windows.Media.ClosedCaptioning.ClosedCaptionEdgeEffect.default) {
                            rowItem.style.textShadow = getTextShadow(captionSettings.fontEffect);
                        }
                    }
                }
            }
        }
    }

    var CaptionSettingsPlugin = WinJS.Class.derive(PlayerFramework.PluginBase, function (options) {
        // constructor
        if (!(this instanceof PlayerFramework.Plugins.CaptionSettingsPlugin)) {
            throw invalidConstruction;
        }

        captionSettings = Windows.Media.ClosedCaptioning.ClosedCaptionProperties;

        PlayerFramework.PluginBase.call(this, options);
    }, {
    }, {
        // static members
    });

    // CaptionSettings methods
    WinJS.Namespace.define("PlayerFramework.CaptionSettings", {
        captionsUpdated: captionsUpdated
    });

    // CaptionsPlugin Exports
    WinJS.Namespace.define("PlayerFramework.Plugins", {
        CaptionSettingsPlugin: CaptionSettingsPlugin

    });

})(PlayerFramework);

