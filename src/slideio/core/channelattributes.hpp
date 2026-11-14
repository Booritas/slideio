// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

namespace slideio
{
    /**@brief Names of the standard channel attributes.
     *
     * Keys of the per-channel objects returned by Scene::getChannelAttributes()
     * that carry the same meaning in every driver that sets them. A driver may
     * add format-specific attributes under other names; these are the ones a
     * consumer can rely on across formats.
     */
    namespace ChannelAttribute
    {
        /**@brief display colour of the channel, a string such as "#RRGGBB". */
        inline constexpr const char* Color = "Color";
        /**@brief name of the channel, a string. */
        inline constexpr const char* Name = "Name";
        /**@brief emission wavelength of the channel. */
        inline constexpr const char* EmissionWavelength = "EmissionWavelength";
        /**@brief excitation wavelength of the channel. */
        inline constexpr const char* ExcitationWavelength = "ExcitationWavelength";
    }
}
