-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 torrentz plugin
-- Copyright (C) Diego de las Heras <ngosang@hotmail.es>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met then
--
--    * Redistributions of source code must retain the above copyright notice,
--      this list of conditions and the following disclaimer.
--    * Redistributions in binary form must reproduce the above copyright
--      notice, this list of conditions and the following disclaimer in the
--      documentation and/or other materials provided with the distribution.
--    * Neither the name of the author nor the names of its contributors may be
--      used to endorse or promote products derived from this software without
--      specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
-- POSSIBILITY OF SUCH DAMAGE.

name = "Torrentz"
version = 1.0
url = "https://torrentz.eu"
supportedCategories = { all = "" }

trackersList = {
    "udp://tracker.openbittorrent.com:80/announce",
    "udp://glotorrents.pw:6969/announce",
    "udp://tracker.leechers-paradise.org:6969",
    "udp://9.rarbg.com:2710/announce",
    "udp://tracker.coppersurfer.tk:6969"
}

function run(what, cat)
    cat = supportedCategories[cat:lower()]
    
    -- initialize trackers for magnet links
    trackers = ""
    for _, tracker in ipairs(trackersList) do
        trackers = trackers .. "&" .. URL.urlencode({ tr = tracker })
    end
    
    for i = 0, 5 do
        -- "what" is already URL.urlencoded
        local data = URL.get(string.format("%s/any?f=%s&p=%d", url, what, i))
        if data == "" then break; end

        tdCounter = nil
        currentItem = nil
        hasResults = false
        HTML.parse(data)
        if not hasResults then break; end
    end
end

function handleStartTag(tag, attrs)
    if tag == "a" then
        if attrs["href"] then
            currentItem = {}
            tdCounter = 0
            currentItem["link"] = "magnet:?xt=urn:btih:" .. attrs["href"]:strip():sub(2) .. trackers
            currentItem["descrLink"] = url .. attrs["href"]:strip()
        end
    elseif tag == "span" then
        if tdCounter then
            tdCounter = tdCounter + 1
            if attrs["class"] and attrs["class"] == "pe" then
                -- hack to avoid Pending
                tdCounter = tdCounter + 2
            end
            
            if tdCounter > 6 then
                -- safety
                tdCounter = nil
            end
        end
    end
end

function handleData(data)
    if tdCounter == 0 then
        if not currentItem["name"] then
            currentItem["name"] = ""
        end
        currentItem["name"] = currentItem["name"] .. data
    elseif tdCounter == 4 then
        if not currentItem["size"] then
            currentItem["size"] = data:strip()
            if currentItem["size"] == "Pending" then
                currentItem["size"] = ""
            end
        end
    elseif tdCounter == 5 then
        if not currentItem["seeds"] then
            currentItem["seeds"] = data:strip():gsub(",", "")
        end
    elseif tdCounter == 6 then
        if not currentItem["leeches"] then
            currentItem["leeches"] = data:strip():gsub(",", "")
        end

        -- display item
        tdCounter = nil
        currentItem["siteUrl"] = url
        currentItem["name"] = currentItem["name"]:split(" »")[1]
        currentItem["link"] = currentItem["link"] .. "&" .. URL.urlencode({ dn = currentItem["name"] })

        newSearchResult(currentItem)
        hasResults = true
    end
end
