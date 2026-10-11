#ifndef WINSPIALERTCLIENT_IMAGECONSTANTS_H
#define WINSPIALERTCLIENT_IMAGECONSTANTS_H

// NOTE: https://en.wikipedia.org/wiki/List_of_file_signatures
#define PNG_SIGNATURE "\x89PNG\r\n\x1a\n"
#define JPEG_SIGNATURE "\xff\xd8\xff"

#define EXIF_ORIENTATION_QUERY L"/app1/ifd/{ushort=274}"
#define JPEG_QUALITY_OPTION L"ImageQuality"
#define FIRST_FRAME 0

#define MAX_DECODE_PIXELS (24.0 * 1000 * 1000)
#define MAX_SOURCE_PIXELS (400.0 * 1000 * 1000)

#endif // WINSPIALERTCLIENT_IMAGECONSTANTS_H
