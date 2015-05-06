// <copyright file="UIElementEventArgs.cs" company="Microsoft Corporation">
// Copyright (c) 2013 Microsoft Corporation All Rights Reserved
// </copyright>
// <author>Michael S. Scherotter</author>
// <email>mischero@microsoft.com</email>
// <date>2013-11-06</date>
// <summary>UI Element event arguments</summary>

namespace Microsoft.PlayerFramework.CC608
{
    using System;
    using Windows.UI.Xaml;

    /// <summary>
    /// UI Element event arguments
    /// </summary>
    public class UIElementEventArgs : EventArgs
    {
        /// <summary>
        /// Initializes a new instance of the UIElementEventArgs class.
        /// </summary>
        /// <param name="uiElement">the UI Element</param>
        public UIElementEventArgs(UIElement uiElement)
        {
            this.UIElement = uiElement;
        }

        /// <summary>
        /// Gets the UI Element
        /// </summary>
        public UIElement UIElement { get; private set; }
    }
}
