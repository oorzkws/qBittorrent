-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 torrentreactor plugin
-- Copyright (C) Gekko Dam Beer <gekko04@users.sourceforge.net>
-- Copyright (C) Christophe Dumez <chris@qbittorrent.org>
-- Copyright (C) Bruno Barbieri <brunorex@gmail.com>
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

name = "TorrentReactor"
version = 1.0
url = "https://torrentreactor.com"
supportedCategories = {
    all = "",
    music = "6",
    movies = "5",
    games = "3",
    software = "2",
    anime = "1",
    tv = "8"
}

function run(what, cat)
    cat = supportedCategories[cat:lower()]

    for page = 0, 8 do
        local data = URL.get(string.format(
            "%s/torrents-search/%s/%d?sort=seeders.desc&type=all&period=none&categories=%s",
            url,
            what,
            page * 35,
            cat
        ))
        if data == "" then break; end

        tdCounter = nil
        currentItem = nil
        id = nil
        hasResults = false
        HTML.parse(data)
        if not hasResults then break; end
    end
end

function handleStartTag(tag, attrs)
    if tag == "a" then
        handleStartTag_a(attrs)
    elseif tag == "td" then
        handleStartTag_td(attrs)
    end
end

function handleData(data)
    local name

    if tdCounter == 1 then name = "size"
    elseif tdCounter == 2 then name = "seeds"
    elseif tdCounter == 3 then name = "leeches"
    else return
    end

    if not currentItem[name] then
        currentItem[name] = ""
    end
    currentItem[name] = currentItem[name] .. data:strip()
end

function handleStartTag_a(attrs)
    local link = attrs["href"]:strip()

    if link:match("^/torrents/%d+.*") then
        currentItem = {}
        currentItem["descrLink"] = url .. link
    elseif link:match("torrentreactor.net/download.php") then
        tdCounter = 0
        currentItem["link"] = link
        currentItem["name"] = URL.unquote(link:split("&")[2]:split("name=")[2]):gsub("+", " ")
    end
end

function handleStartTag_td(attrs)
    if tdCounter then
        tdCounter = tdCounter + 1
        if tdCounter > 3 then
            tdCounter = nil
            -- add item to results
            if currentItem then
                currentItem["siteUrl"] = url
                newSearchResult(currentItem)
                hasResults = true
            end
        end
    end
end
