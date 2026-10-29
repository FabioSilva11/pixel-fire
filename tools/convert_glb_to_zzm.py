#!/usr/bin/env python3
"""Convert a static GLB weapon to the compact mesh format used by Zombie Zone."""

from __future__ import annotations

import argparse
import base64
import json
import math
import mimetypes
import struct
from pathlib import Path


COMPONENTS = {
    5120: ("b", 1),
    5121: ("B", 1),
    5122: ("h", 2),
    5123: ("H", 2),
    5125: ("I", 4),
    5126: ("f", 4),
}
SHAPES = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def read_glb(path: Path) -> tuple[dict, bytes]:
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b"glTF":
        raise ValueError("Input is not a GLB 2.0 file")
    version, total_length = struct.unpack_from("<II", data, 4)
    if version != 2 or total_length > len(data):
        raise ValueError("Only valid GLB version 2 files are supported")
    offset = 12
    document = None
    binary = b""
    while offset + 8 <= total_length:
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_length]
        if len(chunk) != chunk_length:
            raise ValueError("Truncated GLB chunk")
        if chunk_type == 0x4E4F534A:
            document = json.loads(chunk.decode("utf-8"))
        elif chunk_type == 0x004E4942:
            binary = chunk
        offset += chunk_length
    if document is None:
        raise ValueError("GLB JSON chunk is missing")
    return document, binary


def accessor_values(document: dict, binary: bytes, index: int) -> list[tuple]:
    access = document["accessors"][index]
    view = document["bufferViews"][access["bufferView"]]
    fmt, component_size = COMPONENTS[access["componentType"]]
    components = SHAPES[access["type"]]
    packed_size = component_size * components
    stride = view.get("byteStride", packed_size)
    start = view.get("byteOffset", 0) + access.get("byteOffset", 0)
    unpack = struct.Struct("<" + fmt * components).unpack_from
    return [unpack(binary, start + i * stride) for i in range(access["count"])]


def identity_matrix() -> list[float]:
    return [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]


def multiply(a: list[float], b: list[float]) -> list[float]:
    out = [0.0] * 16
    for column in range(4):
        for row in range(4):
            out[column * 4 + row] = sum(a[k * 4 + row] * b[column * 4 + k] for k in range(4))
    return out


def node_local_matrix(node: dict) -> list[float]:
    if "matrix" in node:
        return list(node["matrix"])
    tx, ty, tz = node.get("translation", (0.0, 0.0, 0.0))
    x, y, z, w = node.get("rotation", (0.0, 0.0, 0.0, 1.0))
    sx, sy, sz = node.get("scale", (1.0, 1.0, 1.0))
    return [
        (1 - 2 * (y * y + z * z)) * sx, (2 * (x * y + z * w)) * sx,
        (2 * (x * z - y * w)) * sx, 0.0,
        (2 * (x * y - z * w)) * sy, (1 - 2 * (x * x + z * z)) * sy,
        (2 * (y * z + x * w)) * sy, 0.0,
        (2 * (x * z + y * w)) * sz, (2 * (y * z - x * w)) * sz,
        (1 - 2 * (x * x + y * y)) * sz, 0.0,
        tx, ty, tz, 1.0,
    ]


def transform_point(matrix: list[float], point: tuple[float, ...]) -> tuple[float, float, float]:
    x, y, z = point[:3]
    return (
        matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12],
        matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13],
        matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14],
    )


def transform_normal(matrix: list[float], normal: tuple[float, ...]) -> tuple[float, float, float]:
    # Correctly apply the inverse-transpose of the node's upper-left 3x3 matrix.
    a, b, c = matrix[0], matrix[4], matrix[8]
    d, e, f = matrix[1], matrix[5], matrix[9]
    g, h, i = matrix[2], matrix[6], matrix[10]
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if abs(det) < 1e-12:
        raise ValueError("Mesh node has a singular transform")
    inv = [
        (e * i - f * h) / det, (f * g - d * i) / det, (d * h - e * g) / det,
        (c * h - b * i) / det, (a * i - c * g) / det, (b * g - a * h) / det,
        (b * f - c * e) / det, (c * d - a * f) / det, (a * e - b * d) / det,
    ]
    x, y, z = normal[:3]
    nx = inv[0] * x + inv[3] * y + inv[6] * z
    ny = inv[1] * x + inv[4] * y + inv[7] * z
    nz = inv[2] * x + inv[5] * y + inv[8] * z
    length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return nx / length, ny / length, nz / length


def image_bytes(document: dict, binary: bytes) -> tuple[bytes, str]:
    materials = document.get("materials", [])
    textures = document.get("textures", [])
    images = document.get("images", [])
    source = None
    for material in materials:
        texture_info = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
        if texture_info is not None:
            source = textures[texture_info["index"]].get("source")
            break
    if source is None or source >= len(images):
        raise ValueError("No base-color texture was found for this weapon")
    image = images[source]
    mime = image.get("mimeType") or mimetypes.guess_type(image.get("uri", ""))[0] or "image/png"
    if "bufferView" in image:
        view = document["bufferViews"][image["bufferView"]]
        start = view.get("byteOffset", 0)
        raw = binary[start : start + view["byteLength"]]
    elif image.get("uri", "").startswith("data:"):
        raw = base64.b64decode(image["uri"].split(",", 1)[1])
    else:
        raise ValueError("External image URIs are not supported; use a self-contained GLB")
    suffix = {"image/jpeg": ".jpg", "image/png": ".png", "image/webp": ".webp"}.get(mime)
    if suffix is None:
        raise ValueError(f"Unsupported texture type: {mime}")
    return raw, suffix


def convert(source: Path, output_dir: Path) -> None:
    document, binary = read_glb(source)
    nodes = document.get("nodes", [])
    parents: dict[int, int] = {}
    for parent, node in enumerate(nodes):
        for child in node.get("children", []):
            parents[child] = parent

    mesh_nodes = [i for i, node in enumerate(nodes) if "mesh" in node]
    if not mesh_nodes:
        raise ValueError("GLB contains no mesh node")
    vertices: list[tuple[float, ...]] = []
    indices: list[int] = []
    material_ids: set[int] = set()
    for node_index in mesh_nodes:
        node = nodes[node_index]
        world = identity_matrix()
        chain = []
        current = node_index
        while current in range(len(nodes)):
            chain.append(current)
            if current not in parents:
                break
            current = parents[current]
        for ancestor in reversed(chain):
            world = multiply(world, node_local_matrix(nodes[ancestor]))

        mesh = document["meshes"][node["mesh"]]
        for primitive in mesh.get("primitives", []):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Only triangle-list weapon meshes are supported")
            if primitive.get("material") is not None:
                material_ids.add(primitive["material"])
            attrs = primitive.get("attributes", {})
            positions = accessor_values(document, binary, attrs["POSITION"])
            normals = accessor_values(document, binary, attrs["NORMAL"]) if "NORMAL" in attrs else [(0.0, 1.0, 0.0)] * len(positions)
            uvs = accessor_values(document, binary, attrs["TEXCOORD_0"]) if "TEXCOORD_0" in attrs else [(0.0, 0.0)] * len(positions)
            base = len(vertices)
            for position, normal, uv in zip(positions, normals, uvs):
                p = transform_point(world, position)
                n = transform_normal(world, normal)
                vertices.append((*p, *n, uv[0], uv[1]))
            if "indices" in primitive:
                local_indices = [row[0] for row in accessor_values(document, binary, primitive["indices"])]
            else:
                local_indices = list(range(len(positions)))
            indices.extend(base + int(i) for i in local_indices)

    if len(material_ids) > 1:
        raise ValueError("The runtime material currently supports one textured material per model")
    if not vertices or not indices or len(vertices) > 65535:
        raise ValueError("Mesh is empty or exceeds the runtime 16-bit index limit")
    if any(i < 0 or i >= len(vertices) for i in indices):
        raise ValueError("Mesh index is out of range")

    output_dir.mkdir(parents=True, exist_ok=True)
    flat_vertices = [component for vertex in vertices for component in vertex]
    with (output_dir / "ak47.zzm").open("wb") as output:
        output.write(b"ZZM1")
        output.write(struct.pack("<II", len(vertices), len(indices)))
        output.write(struct.pack("<" + "f" * len(flat_vertices), *flat_vertices))
        output.write(struct.pack("<" + "H" * len(indices), *indices))
    texture, suffix = image_bytes(document, binary)
    (output_dir / f"ak47_texture{suffix}").write_bytes(texture)
    print(f"Converted {source.name}: {len(vertices)} vertices, {len(indices)} indices, texture {len(texture)} bytes")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Path to a static, textured GLB")
    parser.add_argument("output_dir", type=Path, help="Output directory for the converted runtime assets")
    args = parser.parse_args()
    convert(args.source, args.output_dir)


if __name__ == "__main__":
    main()
