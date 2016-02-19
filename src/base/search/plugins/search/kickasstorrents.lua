-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 kickasstorrents plugin
-- Copyright (C) Christophe Dumez <chris@qbittorrent.org>
-- Copyright (C) Diego de las Heras <ngosang@hotmail.es>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
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

name = "Kickass Torrents"
version = 1.0
url = "https://kat.cr"
supportedCategories = { all = "", movies = "Movies", tv = "TV", music = "Music", games = "Games", software = "Applications" }

function run(what, cat)
    local i = 0
    local nresults = 0
    for i = 1, 10 do
        local data = URL.get(url .. "/json.php?" .. URL.urlencode({ q = what, page = i }))
        local dict = JSON.load(data)
        if type(dict) ~= "table" then goto nextpage end
        if tonumber(dict.total_results) <= 0 then return end

        for i, result in ipairs(dict.list) do
            if cat ~= "all" and supportedCategories[cat] ~= result.category then goto nextresult end

            newSearchResult({
                link = result.torrentLink,
                name = result.title,
                size = result.size,
                seeds = result.seeds,
                leeches = result.leeches,
                siteUrl = url,
                descrLink = result.link:gsub("http://", "https://")
            })

            ::nextresult::
        end

        ::nextpage::
    end
end
