/*global HEAPU8, Module, _getInputStringPtr, _doWork*/
function convertToString(ptr, str)
{
  let dest = ptr;
  let length = str.length;
  let i, t;
  for (i=0; i<length; i++)
  {
    t = str.charCodeAt(i);
    if (t<128)
    {
      HEAPU8[(dest++) >> 0] = t;
    }
    else
    {
      HEAPU8[(dest++) >> 0] = (t >> 6) + 192;
      HEAPU8[(dest++) >> 0] = (t & 63) + 128;
    }
  }
  HEAPU8[dest >> 0] = 0;
}

Module =
{
  "preRun": function()
  {
    self.onmessage = function(e)
    {
      convertToString(_getInputStringPtr(), e.data);
      _doWork();
    };
  },
  "noInitialRun": true,
};

