-- Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
--
-- Based on nova2 btdigg plugin
-- Copyright (C) BTDigg team <research@btdigg.org>
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

name = "BTDigg"
version = 1.0
url = "https://btdigg.org"
supportedCategories = { all = "" }

function run(what, cat)
    local i = 0
    local nresults = 0
    for i = 1, 3 do
        local data = URL.get(
            "https://api.btdigg.org/api/public-8e9a50f8335b964f/s01?" 
            .. URL.urlencode({ q = what, p = i }))
        for line in data:gmatch("([^\n]*)\n?") do
            line = line:strip()
            if line:len() == 0 then goto nextline end
            if line:sub(1, 1) == "#" then goto nextline end
                        
            local info_hash, name, files, size, dl, seen = table.unpack(line:split("\t", true))
            name = name:gsub("(|)", "")
            
            newSearchResult({
                link = "magnet:?" ..  URL.urlencode({ xt = "urn:btih:" .. info_hash, dn = name }),
                name = name,
                size = size,
                seeds = tonumber(dl, 10),
                leeches = tonumber(dl, 10),
                siteUrl = url,
                descrLink = url .. "/search?" .. URL.urlencode({ info_hash = info_hash, q = what })
            })
            
            nresults = nresults + 1
            ::nextline::
        end
        
        if nresults == 0 then break end
    end
end
