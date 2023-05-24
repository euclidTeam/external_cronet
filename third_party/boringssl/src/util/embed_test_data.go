// Copyright (c) 2017, Google Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

// embed_test_data generates a C++ source file which exports a function,
// GetTestData, which looks up the specified data files.
package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"strings"
)

var fileList = flag.String("file-list", "", "if not empty, the path to a file containing a newline-separated list of files, to work around Windows command-line limits")

func quote(in []byte) string {
	var lastWasHex bool
	var buf bytes.Buffer
	buf.WriteByte('"')
	for _, b := range in {
		var wasHex bool
		switch b {
		case '\a':
			buf.WriteString(`\a`)
		case '\b':
			buf.WriteString(`\b`)
		case '\f':
			buf.WriteString(`\f`)
		case '\n':
			buf.WriteString(`\n`)
		case '\r':
			buf.WriteString(`\r`)
		case '\t':
			buf.WriteString(`\t`)
		case '\v':
			buf.WriteString(`\v`)
		case '"':
			buf.WriteString(`\"`)
		case '\\':
			buf.WriteString(`\\`)
		default:
			// Emit printable ASCII characters, [32, 126], as-is to minimize
			// file size. However, if the previous character used a hex escape
			// sequence, do not emit 0-9 and a-f as-is. C++ interprets "\x123"
			// as a single (overflowing) escape sequence, rather than '\x12'
			// followed by '3'.
			isHexDigit := ('0' <= b && b <= '9') || ('a' <= b && b <= 'f') || ('A' <= b && b <= 'F')
			if 32 <= b && b <= 126 && !(lastWasHex && isHexDigit) {
				buf.WriteByte(b)
			} else {
				fmt.Fprintf(&buf, "\\x%02x", b)
				wasHex = true
			}
		}
		lastWasHex = wasHex
	}
	buf.WriteByte('"')
	return buf.String()
}

func main() {
	flag.Parse()

	var files []string
	if len(*fileList) != 0 {
		data, err := os.ReadFile(*fileList)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error reading %s: %s.\n", *fileList, err)
			os.Exit(1)
		}
		files = strings.FieldsFunc(string(data), func(r rune) bool { return r == '\r' || r == '\n' })
	}

	files = append(files, flag.Args()...)

	fmt.Printf(`/* Copyright (c) 2017, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

/* This file is generated by:
`)
	fmt.Printf(" *   go run util/embed_test_data.go")
	for _, arg := range files {
		fmt.Printf(" \\\n *       %s", arg)
	}
	fmt.Printf(" */\n")

	fmt.Printf(`
/* clang-format off */

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>


`)

	// MSVC limits the length of string constants, so we emit an array of
	// them and concatenate at runtime. We could also use a single array
	// literal, but this is less compact.
	const chunkSize = 8192

	for i, arg := range files {
		data, err := os.ReadFile(arg)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error reading %s: %s.\n", arg, err)
			os.Exit(1)
		}
		fmt.Printf("static const size_t kLen%d = %d;\n\n", i, len(data))

		fmt.Printf("static const char *kData%d[] = {\n", i)
		for len(data) > 0 {
			chunk := chunkSize
			if chunk > len(data) {
				chunk = len(data)
			}
			fmt.Printf("    %s,\n", quote(data[:chunk]))
			data = data[chunk:]
		}
		fmt.Printf("};\n")
	}

	fmt.Printf(`static std::string AssembleString(const char **data, size_t len) {
  std::string ret;
  for (size_t i = 0; i < len; i += %d) {
    size_t chunk = std::min(static_cast<size_t>(%d), len - i);
    ret.append(data[i / %d], chunk);
  }
  return ret;
}

/* Silence -Wmissing-declarations. */
std::string GetTestData(const char *path);

std::string GetTestData(const char *path) {
`, chunkSize, chunkSize, chunkSize)
	for i, arg := range files {
		fmt.Printf("  if (strcmp(path, %s) == 0) {\n", quote([]byte(arg)))
		fmt.Printf("    return AssembleString(kData%d, kLen%d);\n", i, i)
		fmt.Printf("  }\n")
	}
	fmt.Printf(`  fprintf(stderr, "File not embedded: %%s.\n", path);
  abort();
}
`)

}
