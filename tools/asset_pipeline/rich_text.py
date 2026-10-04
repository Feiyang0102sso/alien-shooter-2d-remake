"""Convert original page controls to readable UTF-8 text and back for verification."""

ALIGNMENTS = {"[": "left", "|": "center", "]": "right"}
ICON_COUNT = 21


def escape_text(text):
    """Keep literal markup characters distinct from layout tags."""
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def encode_rich_text(source):
    """Store the title on line one and express every body line break explicitly."""
    content, terminator, unused = source.partition("}")
    if not terminator:
        raise ValueError("Rich text page has no terminator")
    title, separator, body = content.partition("\r\n")
    if not separator or not title:
        raise ValueError("Expected a plain page title followed by CRLF")
    for character in "[|]{^\r\n":
        if character in title:
            raise ValueError("Unexpected control in page title")
    fragments = [escape_text(title), "\n"]
    cursor = 0
    while cursor < len(body):
        character = body[cursor]
        if character in ALIGNMENTS:
            fragments.append("<align:" + ALIGNMENTS[character] + ">")
        elif character == "{":
            fragments.append("\n")
        elif character == "^":
            cursor += 1
            if cursor >= len(body):
                raise ValueError("Missing rich text image ID")
            image_id = ord(body[cursor]) - 32
            if not 0 <= image_id < ICON_COUNT:
                raise ValueError(f"Invalid rich text image ID: {image_id}")
            # The original image command always starts another line, even after a break.
            fragments.append("\n<img:" + str(image_id) + ">")
        elif character in "\r\n\t":
            raise ValueError("Unexpected whitespace control in original page body")
        else:
            fragments.append(escape_text(character))
        cursor += 1
    return "".join(fragments)


def decode_rich_text(markup):
    """Reconstruct source controls to verify that export preserves page layout."""
    title, separator, body = markup.partition("\n")
    if not title or not separator:
        raise ValueError("Rich text page requires a title line")
    fragments = []
    cursor = 0
    text = title + "\n" + body
    entities = {"&amp;": "&", "&lt;": "<", "&gt;": ">"}
    while cursor < len(text):
        character = text[cursor]
        if character == "\n":
            if cursor == len(title):
                fragments.append("\r\n")
            elif text.startswith("<img:", cursor + 1):
                # The following image command already includes the source line break.
                pass
            else:
                fragments.append("{")
        elif character == "<":
            end = text.find(">", cursor)
            if end < 0:
                raise ValueError("Unclosed rich text tag")
            tag = text[cursor + 1:end]
            if tag.startswith("align:") and tag[6:] in ALIGNMENTS.values():
                for control, alignment in ALIGNMENTS.items():
                    if alignment == tag[6:]:
                        fragments.append(control)
            elif tag.startswith("img:") and tag[4:].isascii() and tag[4:].isdigit():
                image_id = int(tag[4:])
                if not 0 <= image_id < ICON_COUNT or cursor <= len(title) + 1 or text[cursor - 1] != "\n":
                    raise ValueError("Image must start a body line and reference an existing icon")
                fragments.append("^" + chr(image_id + 32))
            else:
                raise ValueError(f"Unknown rich text tag: {tag}")
            cursor = end
        elif character == "&":
            end = text.find(";", cursor)
            entity = text[cursor:end + 1]
            if entity not in entities:
                raise ValueError("Unknown rich text entity")
            fragments.append(entities[entity])
            cursor = end
        else:
            fragments.append(character)
        cursor += 1
    fragments.append("}")
    return "".join(fragments)
