"""Author the visual reflection fixture and encode matched readbacks."""
import math
import re
import struct
import zlib

LOCAL_PROJECT = "Reflection Gallery - Local.hproj"
GLOBAL_PROJECT = "Reflection Gallery - Global.hproj"
LOCAL_SCENE = "Scenes/ReflectionGallery-Local.hazel"
GLOBAL_SCENE = "Scenes/ReflectionGallery-Global.hazel"
POSES = 33
SIZE = (640, 360)
EXPOSURE = .1
CAMERA = (0, 3.35, 9.3)
LOOK = (0, 1.18, -.6)
FOV = 50
BACK = -3.6
CAPTURE_X = 2.65
CAPTURE_Y = 1.65
CAPTURE_Z = .65
PROJECTION_OFFSET = (0, .11, .3)
PROJECTION_SIZE = (3.85, 1.84, 4.55)

WARM = ((.024, .004, .001), (.97, .21, .011), (1, .83, .39))
COOL = ((.003, .024, .043), (.016, .61, .99), (.70, .93, 1))


def wall_tiles(warm):
    """Back-wall rectangles in world coordinates, shared by the scene and HDR."""
    left = -6.25 if warm else .20
    right = -.20 if warm else 6.25
    columns, rows = (9, 1) if warm else (6, 3)
    dx, dy = (right - left) / columns, 3.07 / rows
    for y in range(rows):
        for x in range(columns):
            color = (WARM if warm else COOL)[(x + y) % 2]
            if warm and x == 6:
                color = WARM[2]
            if not warm and (x, y) in ((1, 2), (4, 0)):
                color = COOL[2]
            yield left + x * dx, .23 + y * dy, dx, dy, color


def write_radiance(path, warm):
    side = -1 if warm else 1
    capture = (side * CAPTURE_X, CAPTURE_Y, CAPTURE_Z)
    center = tuple(a + b for a, b in zip(capture, PROJECTION_OFFSET))
    low = tuple(a - b for a, b in zip(center, PROJECTION_SIZE))
    high = tuple(a + b for a, b in zip(center, PROJECTION_SIZE))
    tiles = list(wall_tiles(warm))
    palette = WARM if warm else COOL
    width, height = 512, 256
    content = bytearray(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode())
    azimuth = [2 * math.pi * ((x + .5) / width - .5) for x in range(width)]
    horizontal = [(math.cos(a), math.sin(a)) for a in azimuth]
    for y in range(height):
        inclination = math.pi * (y + .5) / height
        sine, cosine = math.sin(inclination), math.cos(inclination)
        for sx, sz in horizontal:
            ray = (sine * sx, cosine, sine * sz)
            distances = [(high[i] - capture[i]) / d if d > 0 else (low[i] - capture[i]) / d
                         for i, d in enumerate(ray)]
            axis = min(range(3), key=distances.__getitem__)
            point = [p + d * distances[axis] for p, d in zip(capture, ray)]
            rgb = (.12, .13, .15)
            if axis == 2 and ray[2] < 0:
                rgb = (.043, .048, .058)
                for x0, y0, dx, dy, color in tiles:
                    if x0 <= point[0] < x0 + dx and y0 <= point[1] < y0 + dy:
                        rgb = color
                        break
            elif axis == 1 and ray[1] > 0 and abs(point[0] - capture[0]) < 1.05 and abs(point[2] + .4) < .55:
                rgb = palette[2]
            elif axis == 0 and abs(point[2] - .4) < .35 and .65 < point[1] < 2.85:
                rgb = palette[1]
            exponent = math.frexp(max(rgb))[1]
            scale = 256 / math.ldexp(1., exponent)
            content.extend((*[int(channel * scale) for channel in rgb], exponent + 128))
    path.write_bytes(content)


def write_project(root):
    assets = root / "Assets"
    materials = assets / "ReflectionStudy/Materials"
    materials.mkdir()
    template = (assets / "Native/Direct_v1/Surface.hmat").read_text()
    scene = f"""SceneVersion: 3
Scene: Reflection Gallery
Presentation:
  Mode: SceneReferred
  Exposure: {EXPOSURE}
  Background: [.023, .029, .041, 1]
  Environment:
    Asset: Reference/IBL_v1/Constant.henv
    Yaw: 0
    Gain: 1
    Lighting: true
    Background: false
Entities:
"""
    entities = []

    def material(name, base, metallic=0, roughness=.6, emission=(0, 0, 0)):
        fields = {"Name": name, "BaseColor": list((*base, 1)), "Metallic": metallic,
                  "Roughness": roughness, "Emissive": list(emission)}
        text = template
        for key, value in fields.items():
            text = re.sub(rf"^  {key}: .*", f"  {key}: {value}", text, flags=re.MULTILINE)
        path = materials / (name + ".hmat")
        path.write_text(text)
        return path.relative_to(assets).as_posix()

    def add(name, position, scale, fields, rotation=(0, 0, 0)):
        i = len(entities)
        entities.append(f"""  - Entity: {100 + i}
    Relationship:
      Parent: 0
      Order: {i}
    TagComponent:
      Tag: {name}
    TransformComponent:
      Translation: {list(position)}
      Rotation: {list(rotation)}
      Scale: {list(scale)}
{fields}""")

    def mesh(name, position, scale, surface, sphere=False):
        reference = "Reference/Direct_v1/Sphere.hmesh" if sphere else "Native/Direct_v1/UnitCube.hmesh"
        slot = "000000000000000e" if sphere else "0000000000000007"
        add(name, position, scale, f"""    MeshRendererComponent:
      Version: 1
      Mesh: {reference}
      Materials:
        - Slot: {slot}
          Material: {surface}
""")

    pitch = -math.atan2(CAMERA[1] - LOOK[1], CAMERA[2] - LOOK[2])
    add("Matched center camera", CAMERA, (1, 1, 1), f"""    CameraComponent:
      Camera:
        ProjectionType: 0
        PerspectiveFOV: {math.radians(FOV)}
        PerspectiveNear: .1
        PerspectiveFar: 50
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
""", (pitch, 0, 0))

    neutral = material("Graphite", (.105, .12, .15))
    light_trim = material("Stone", (.30, .32, .36))
    chrome = material("Chrome", (.93, .94, .96), 1, .045)
    rough = material("Satin metal", (.77, .80, .85), 1, .38)
    mirror = material("Mirror strip", (.92, .94, .97), 1, .045)
    mesh("Floor", (0, -.10, .2), (13.1, .2, 10), neutral)
    mesh("Back wall", (0, 1.76, BACK - .16), (13.1, 3.68, .22), neutral)
    mesh("Wall division", (0, 1.76, BACK + .05), (.24, 3.65, .18), light_trim)
    mesh("Shared mirror rail", (0, .57, 2.03), (11.9, .14, 1.48), mirror)
    mesh("Mirror rail base", (0, .23, 2.03), (12.0, .46, 1.57), neutral)
    for warm, side, name, palette in ((True, -1, "Warm", WARM), (False, 1, "Cool", COOL)):
        mesh(f"{name} outer wall", (side * 6.55, 1.75, .25), (.16, 3.5, 7.75), neutral)
        wall_materials = {
            color: material(f"{name} panel {i + 1}", (.006, .006, .006), roughness=.8, emission=color)
            for i, color in enumerate(palette)
        }
        for i, (x, y, dx, dy, color) in enumerate(wall_tiles(warm)):
            mesh(f"{name} registration tile {i + 1}", (x + dx / 2, y + dy / 2, BACK - .025),
                 (dx, dy, .05), wall_materials[color])
        mesh(f"{name} chrome sphere", (side * 2.08, 1.11, -.43), (2.18, 2.18, 2.18), chrome, True)
        mesh(f"{name} satin sphere", (side * 4.12, .92, -.72), (1.80, 1.80, 1.80), rough, True)
        add(f"{name} gallery fill", (side * 3.3, 3.7, .85), (1, 1, 1), """    PointLightComponent:
      Color: [1, .96, .88]
      Intensity: 13
      Enabled: true
      Range: 8
""")
        add(f"{name} probe capture", (side * CAPTURE_X, CAPTURE_Y, CAPTURE_Z), (1, 1, 1), f"""    ReflectionProbeComponent:
      Environment: ReflectionStudy/{name}.henv
      InfluenceHalfSize: [3.65, 1.66, 3.7]
      ProjectionOffset: {list(PROJECTION_OFFSET)}
      ProjectionHalfSize: {list(PROJECTION_SIZE)}
      BlendDistance: .54
      Enabled: true
""")

    scene += "".join(entities)
    for filename, scene_path, enabled in ((LOCAL_PROJECT, LOCAL_SCENE, True), (GLOBAL_PROJECT, GLOBAL_SCENE, False)):
        content = scene if enabled else re.sub(
            r"(    ReflectionProbeComponent:\n(?:      [^\n]+\n)*?      Enabled: )true", r"\1false", scene)
        (assets / scene_path).write_text(content)
        label = "Local probes" if enabled else "Global only"
        (root / filename).write_text(f"""Project:
  Version: 1
  Name: Reflection Gallery - {label}
  ScriptProject: RenderingReference
  AuthoringVersion: 1
  StartScene: {scene_path}
  AssetDirectory: Assets
  ScriptModulePath: Scripts/Binaries/RenderingReference.dll
""")


def write_motion(path, folder, read_pixels):
    """Pack the same camera sequence side by side into a looping RGB APNG."""
    width, height = SIZE
    output_height = height // 2
    frame_order = [*range(POSES), *range(POSES - 2, 0, -1)]

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    def encode(value):
        linear = min(max(value, 0), 1)
        return round(255 * (12.92 * linear if linear <= .0031308 else 1.055 * linear ** (1 / 2.4) - .055))

    def rows(images):
        for y in reversed(range(0, height, 2)):
            yield b"\0" + b"".join(bytes(encode(pixel[c]) for pixel in image[y * width:(y + 1) * width:2]
                                        for c in range(3)) for image in images)

    compressed = []
    for pose in range(POSES):
        images = [read_pixels(folder / f"{mode}-{pose}.bin") for mode in ("global", "local")]
        if any(len(image) != width * height for image in images):
            raise RuntimeError("Incomplete reflection gallery motion frame")
        compressed.append(zlib.compress(b"".join(rows(images))))

    with path.open("wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        file.write(chunk(b"IHDR", struct.pack(">2I5B", width, output_height, 8, 2, 0, 0, 0)))
        file.write(chunk(b"acTL", struct.pack(">II", len(frame_order), 0)))
        sequence = 0
        for index, pose in enumerate(frame_order):
            file.write(chunk(b"fcTL", struct.pack(">IIIIIHHBB", sequence, width, output_height, 0, 0, 1, 12, 0, 0)))
            sequence += 1
            if index == 0:
                file.write(chunk(b"IDAT", compressed[pose]))
            else:
                file.write(chunk(b"fdAT", struct.pack(">I", sequence) + compressed[pose]))
                sequence += 1
        file.write(chunk(b"IEND", b""))
