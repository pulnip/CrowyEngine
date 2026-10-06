"""PaintLab's shots for the MintChoco paint deck, read by paint_record.py.

A clip is about 5 s of counted 60 Hz frames. `setup` runs before frame 0,
`events` maps a frame index to port calls made before that frame, `animate`
takes (frame, frame count) and returns calls for each frame, `captions` are
(frame, text) pairs burnt in by ffmpeg, `stills` name frames to keep as PNG.
A shot with `frames` instead is a still sheet: each entry's calls, then one
picture. Positions are MintChoco centimetres (X forward, Y right, Z up).
"""

import math

# an open patch of floor, clear of every block
SPOT = (100.0, 0.0, 0.0)
CRATE = (-500.0, -300.0, 150.0)
SPHERE = (500.0, 350.0, 120.0)
# the shape lab panel and its caption, as the window draws them
PANEL_CROP = (1240, 8, 672, 732)
GRID_THETA = (0.0, 50.0, 60.0, 70.0)
GRID_SPEED = (500.0, 1500.0, 3000.0, 4800.0)
GRID_SIDE = len(GRID_THETA)
assert len(GRID_SPEED) == GRID_SIDE
GRID_PITCH = 600.0
# paintGridTileIndex's block count in PaintStage.cpp
FIRST_TILE = 6


def lab(path, value, target="lab"):
    return {"cmd": "set_property", "args": {"target": target, "path": path, "value": value}}


def look(path, value):
    return lab(path, value, "look")


def splat(path, value):
    return lab(path, value, "splat")


def stage(name, path, value):
    return lab(path, value, f"stage.{name}")


def crowy(p):
    return [0.01 * p[1], 0.01 * p[2], 0.01 * p[0]]


def camera(eye, target, fov=None):
    e = crowy(eye)
    t = crowy(target)
    f = [t[i] - e[i] for i in range(3)]
    n = math.sqrt(sum(x * x for x in f))
    f = [x / n for x in f]
    calls = [
        lab("position", e, "camera"),
        lab("yaw", math.atan2(f[0], f[2]), "camera"),
        lab("pitch", math.asin(-f[1]), "camera"),
    ]
    if fov:
        calls.append(lab("fovY", math.radians(fov), "camera"))
    return calls


def add(a, b):
    return [a[i] + b[i] for i in range(3)]


def incident(point, theta_deg, distance=600.0, azimuth_deg=0.0):
    """the origin of a ball reaching `point` at theta from the floor's normal"""
    t = math.radians(theta_deg)
    a = math.radians(azimuth_deg)
    d = [math.sin(t) * math.cos(a), math.sin(t) * math.sin(a), -math.cos(t)]
    return [point[i] - d[i] * distance for i in range(3)]


def fire(origin, target, **kw):
    args = {"origin": list(origin), "target": list(target)}
    args.update(kw)
    return {"cmd": "paint_fire", "args": args}


def shot_at(point, theta=0.0, azimuth=0.0, **kw):
    return fire(incident(point, theta, 600.0, azimuth), point, **kw)


def orbit(center, radius, height, start_deg, sweep_deg, fov):
    def animate(frame, total):
        a = math.radians(start_deg + sweep_deg * frame / max(total - 1, 1))
        eye = [center[0] + radius * math.cos(a), center[1] + radius * math.sin(a), center[2] + height]
        return camera(eye, center, fov)
    return animate


def ramp(path, start, end, first, last, target="lab"):
    """a property eased from start to end between two of the clip's frames"""
    def value(frame):
        u = min(max((frame - first) / max(last - first, 1), 0.0), 1.0)
        u = u * u * (3.0 - 2.0 * u)
        return start + (end - start) * u
    return path, value, target


def animate_ramps(ramps):
    def animate(frame, total):
        return [lab(path, value(frame), target) for path, value, target in ramps]
    return animate


MOP = {"brush": "MopT", "volume": 2.4, "ballRadius": 12.0}
# the deck's reference contact: v = (1500, 0, -2500), MopT
CONTACT_V = (1500.0, 0.0, -2500.0)
CONTACT_THETA = math.degrees(math.atan2(CONTACT_V[0], -CONTACT_V[2]))
CONTACT_SPEED = math.hypot(CONTACT_V[0], CONTACT_V[2])
MOP_CONTACT = dict(speed=CONTACT_SPEED, **MOP)
FLOOR_EYE = (SPOT[0] - 170.0, SPOT[1] - 240.0, 230.0)


def t1_shots():
    crate_eye = (200.0, 400.0, 650.0)
    edge = add(CRATE, (120.0, 0.0, 150.0))
    return [
        {
            "name": "t1_crate_relayout",
            "seconds": 5.0,
            "setup": camera(crate_eye, add(CRATE, (0.0, 0.0, -30.0)), 40)
            + [lab("view", "Islands"), lab("panel", "Atlas"), lab("panelChannel", "Islands"), lab("selected", 1)],
            "events": {90: [stage("crate", "front", True)], 180: [stage("crate", "right", True)]},
            "captions": [(0, "Up만: island 1개"), (90, "+Front: 재포장"), (180, "+Right: 아틀라스 하나에 3개")],
            "stills": {"t1_crate_atlas": 299},
        },
        {
            "name": "t1_sphere_orbit",
            "seconds": 5.0,
            "setup": [lab("view", "Islands")],
            "animate": orbit(SPHERE, 380.0, 300.0, -90.0, 360.0, 45),
            "captions": [(0, "지배축(dominant axis)으로 6방향 island 선택")],
            "stills": {"t1_sphere_islands": 60},
        },
        {
            "name": "t1_edge_splat",
            "seconds": 5.0,
            "setup": camera((-150.0, -560.0, 520.0), (-380.0, -300.0, 280.0), 42)
            + [stage("crate", "front", True), lab("view", "Lit")],
            "events": {
                20: [fire(incident(edge, 45.0, 900.0, 180.0), edge, speed=3000.0, team=0, seed=31, lead=0.3,
                          brush="MopT", volume=1.0)],
                150: [lab("compareView", "Islands"), lab("split", 0.5)],
            },
            "captions": [(0, "모서리를 넘는 스플랫"), (150, "Lit | Islands")],
        },
    ]


def t2_shots():
    target = list(SPOT)
    theta = CONTACT_THETA
    return [
        {
            "name": "t2_score_wipe",
            "seconds": 5.0,
            "setup": camera(FLOOR_EYE, SPOT, 45)
            + [lab("view", "Lit"), lab("compareView", "Score"), lab("split", 1.0)],
            "events": {10: [fire(incident(target, theta, 1200.0), target, team=0, seed=11035, lead=0.4, **MOP_CONTACT)]},
            "animate": animate_ramps([ramp("split", 1.0, 0.0, 90, 210)]),
            "captions": [(0, "GPU: RenderTarget에 스탬프"), (90, "CPU: 25 cm 셀 그리드가 점수")],
            "stills": {"t2_lit_score": 150},
        },
        {
            "name": "t2_overlap",
            "seconds": 5.0,
            "setup": camera(FLOOR_EYE, SPOT, 45)
            + [shot_at(target, theta, speed=CONTACT_SPEED, team=0, seed=11035, **MOP), lab("view", "Score")],
            "events": {
                60: [shot_at(add(target, (80.0, 60.0, 0.0)), theta, speed=CONTACT_SPEED, team=1, seed=7, lead=0.3, **MOP)],
                180: [shot_at(add(target, (-40.0, -70.0, 0.0)), theta, speed=CONTACT_SPEED, team=1, seed=8, lead=0.3, **MOP)],
            },
            "captions": [(0, "민트 셀"), (60, "초코가 덮으면 셀 주인이 바뀜")],
        },
    ]


def t3_shots():
    seam_y = SPOT[1] + 16.0
    seam = [
        shot_at(list(SPOT), seed=2024, team=0),
        shot_at(add(SPOT, (0.0, 35.0, 0.0)), seed=99, team=1),
    ]
    sphere_shots = []
    count = 44
    for i in range(count):
        # a Fibonacci sphere of directions, the ones under the floor left out
        z = 1.0 - 2.0 * (i + 0.5) / count
        if z < -0.35:
            continue
        r = math.sqrt(1.0 - z * z)
        a = i * math.pi * (3.0 - math.sqrt(5.0))
        d = (r * math.cos(a), r * math.sin(a), z)
        origin = [SPHERE[k] + d[k] * 700.0 for k in range(3)]
        sphere_shots.append(fire(origin, list(SPHERE), team=0, seed=300 + i, brush="MopT", volume=2.0))
    lobe_steps = [
        (0, [], "모든 lobe: Substrate slab"),
        (43, [look("sky", False)], "− sky reflection"),
        (86, [look("coat", False)], "− clear coat (F0 0.04, rough 0.12)"),
        (129, [look("fuzz", False)], "− fuzz"),
        (172, [look("sss", False)], "− SSS (wrap 근사)"),
        (215, [look("haze", False)], "− haze (2nd lobe)"),
        (258, [look("specular", False)], "diffuse만"),
    ]
    sphere_eye = (SPHERE[0] - 190.0, SPHERE[1] - 220.0, 230.0)
    # the sun near the eye, a little aside, so the highlights face the camera
    sun = [look("sunAzimuth", 250.0), look("sunElevation", 30.0)]
    return [
        {
            "name": "t3_edge_modes",
            "seconds": 5.0,
            "setup": camera((SPOT[0], SPOT[1] - 60.0, 45.0), (SPOT[0], SPOT[1] - 22.0, 0.0), 30)
            + [shot_at(list(SPOT), seed=2024, team=0), look("edgeMode", "Nearest")],
            "events": {100: [look("edgeMode", "Bilinear")], 200: [look("edgeMode", "SignedDistance")]},
            "captions": [(0, "Nearest: 텍셀 계단"), (100, "Bilinear: id가 섞여 없는 팀이 생김"), (200, "Signed distance: 서브텍셀 경계")],
            "stills": {"t3_edge_nearest": 50, "t3_edge_bilinear": 150, "t3_edge_sdf": 250},
        },
        {
            "name": "t3_team_blend",
            "seconds": 5.0,
            "setup": camera((SPOT[0], seam_y, 30.0), (SPOT[0] + 0.01, seam_y, 0.0), 30)
            + seam + [look("teamBlend", "Naive")],
            "events": {150: [look("teamBlend", "ConsumedCoverage")]},
            "captions": [(0, "Naive lerp: 이음매로 바닥색이 샘"), (150, "Consumed coverage: α = cov / (1 − S)")],
            "stills": {"t3_blend_naive": 75, "t3_blend_consumed": 225},
            # the seam band is one pixel wide by design: twelve times larger
            "zoom": (374, 752, 160, 90),
        },
        {
            "name": "t3_lobes",
            "seconds": 5.0,
            "setup": camera(sphere_eye, SPHERE, 34) + sphere_shots + sun,
            "settle": 4,
            "events": {f: calls for f, calls, _ in lobe_steps},
            "captions": [(f, text) for f, _, text in lobe_steps],
            "stills": {"t3_lobes_all": 20, "t3_lobes_diffuse": 290},
        },
        {
            "name": "t3_collapse",
            "seconds": 5.0,
            "setup": camera(sphere_eye, SPHERE, 34) + sphere_shots + sun,
            "settle": 4,
            "events": {150: [look("blendableGBuffer", True)]},
            "captions": [(0, "Substrate: coat slab over body slab"), (150, "Blendable GBuffer (UE 소스 기준 추정): closure 1개")],
            "stills": {"t3_layered": 75, "t3_collapsed": 225},
        },
    ]


def t4_shots():
    panel = [lab("panel", "ShapeLab"), splat("seed", 5), splat("theta", 50.0), splat("shapeStage", 0.0)]
    return [
        {
            "name": "t4_shape_morph",
            "seconds": 5.0,
            "setup": panel,
            "animate": animate_ramps([ramp("shapeStage", 0.0, 8.0, 0, 264, "splat")]),
            "crop": PANEL_CROP,
        },
        {
            "name": "t4_theta_sweep",
            "seconds": 5.0,
            "setup": panel[:2] + [splat("theta", 0.0), splat("shapeStage", 8.0)],
            "animate": animate_ramps([ramp("theta", 0.0, 70.0, 20, 280, "splat")]),
            "crop": PANEL_CROP,
        },
    ]


def t5_shots():
    eye = (SPOT[0], SPOT[1] - 110.0, 120.0)
    base = camera(eye, SPOT, 50) + [lab("view", "Height"), lab("panel", "Atlas"), lab("panelChannel", "Profile"), lab("selected", 0)]
    def one(**kw):
        return shot_at(list(SPOT), seed=2024, team=0, **kw)
    return [
        {
            "name": "t5_stack",
            "seconds": 5.0,
            "setup": base,
            "events": {30: [one()], 90: [one()], 150: [one()], 210: [one()]},
            "captions": [(0, "같은 자리에 반복"), (30, "1발: G 0.35"), (90, "2발: 0.70"), (150, "3발: 1.0 포화"), (210, "4발: 그대로 1.0")],
            "stills": {"t5_height_two": 140},
        },
        {
            "name": "t5_cross_team",
            "seconds": 5.0,
            "setup": base + [one(), one()],
            "events": {
                60: [shot_at(add(SPOT, (30.0, 0.0, 0.0)), seed=99, team=1)],
                180: [shot_at(add(SPOT, (-30.0, 0.0, 0.0)), seed=7, team=7)],
            },
            "captions": [(0, "민트 2발"), (60, "초코: 팀과 무관하게 쌓임"), (180, "지우개(id 7)만 깎음")],
        },
    ]


def t6_shots():
    eye = (SPOT[0] - 50.0, SPOT[1] - 50.0, 50.0)
    stack = [shot_at(list(SPOT), seed=2024, team=0), shot_at(list(SPOT), seed=2025, team=0)]
    painted = stack + [
        shot_at(add(SPOT, (70.0, 40.0, 0.0)), seed=99, team=1),
        shot_at(add(SPOT, (-60.0, 50.0, 0.0)), seed=5, team=0),
        shot_at(add(SPOT, (20.0, -70.0, 0.0)), seed=6, team=1),
        shot_at(add(SPOT, (20.0, -70.0, 0.0)), seed=16, team=1),
    ]
    return [
        {
            "name": "t6_filters",
            "seconds": 5.0,
            "setup": camera(eye, SPOT, 40) + stack + [lab("view", "Normal"), look("heightFilter", "Nearest")],
            "events": {100: [look("heightFilter", "Bilinear")], 200: [look("heightFilter", "BSpline")]},
            "captions": [(0, "Nearest + 중앙차분"), (100, "Bilinear + 중앙차분"), (200, "12×12 cubic B-spline + 해석적 기울기")],
            "stills": {"t6_normal_nearest": 50, "t6_normal_bspline": 250},
        },
        {
            "name": "t6_normal_to_lit",
            "seconds": 5.0,
            "setup": camera(eye, SPOT, 40) + stack + [lab("view", "Normal"), lab("compareView", "Lit"), lab("split", 1.0)],
            "animate": animate_ramps([
                ramp("split", 1.0, 0.0, 10, 120),
                ramp("sunElevation", 60.0, 10.0, 130, 290, "look"),
            ]),
            "captions": [(0, "Normal = cross(Pu + hu·n, Pv + hv·n)"), (130, "Lit, 낮아지는 태양")],
        },
        {
            "name": "t6_final_orbit",
            "seconds": 5.0,
            "setup": painted + [look("sunElevation", 32.0)],
            "animate": orbit(list(SPOT), 260.0, 130.0, -120.0, 120.0, 40),
            "stills": {"t6_final": 150},
        },
    ]


def t7_shots():
    target = list(SPOT)
    theta = CONTACT_THETA
    contact = dict(speed=CONTACT_SPEED, team=0, seed=11035, splash=True, **MOP)
    return [
        {
            "name": "t7_splash",
            "seconds": 5.0,
            "fps": 60,
            "setup": camera((SPOT[0] + 180.0, SPOT[1] - 650.0, 230.0), (SPOT[0] + 180.0, SPOT[1], 30.0), 50)
            + [lab("slowMotion", 2), lab("splashDebug", True), lab("labels", False)],
            "events": {0: [fire(incident(target, theta, 1500.0), target, lead=0.25, **contact)]},
            "captions": [(0, "v (1500, 0, −2500): Forward 9 · Side 5 · Back 2"), (120, "물방울은 첫 충돌에서 착지 (1/2 속도)")],
            "stills": {"t7_flight": 110},
        },
        {
            "name": "t7_score_vs_picture",
            "seconds": 5.0,
            "setup": camera((SPOT[0] + 150.0, SPOT[1] - 380.0, 650.0), (SPOT[0] + 150.0, SPOT[1], 0.0), 55)
            + [lab("splashDebug", True), lab("labels", False)],
            "events": {
                0: [fire(incident(target, theta, 1500.0), target, lead=0.2, **contact)],
                150: [lab("compareView", "Score"), lab("split", 0.5)],
            },
            "captions": [(0, "노란 링: phantom 착지(점수) · 흰 원: 물방울 자국(그림)"), (150, "Lit | Score: 점수는 GPU를 읽지 않음")],
            "stills": {"t7_rings": 140, "t7_split": 290},
        },
        {
            "name": "t7_gate",
            "seconds": 5.0,
            "setup": camera((SPOT[0], SPOT[1] - 330.0, 230.0), (SPOT[0] + 30.0, SPOT[1], 0.0), 60)
            + [lab("slowMotion", 2), lab("splashDebug", True), lab("labels", False)],
            "events": {0: [
                fire(add(SPOT, (-150.0 - 200.0, 0.0, 150.0)), add(SPOT, (-150.0, 0.0, 0.0)), speed=500.0, team=0, seed=1, splash=True, lead=0.3),
                fire(add(SPOT, (150.0 - 200.0, 0.0, 600.0)), add(SPOT, (150.0, 0.0, 0.0)), speed=math.hypot(400.0, 1200.0), team=1, seed=2, splash=True, lead=0.3),
            ]},
            # +X runs to the left in this view: the fast contact is the left one
            "captions": [(0, "왼쪽: 법선 속도 1200 cm/s, splash   오른쪽: 300 cm/s < 400, splash 없음")],
        },
    ]


def e2e_shots():
    target = list(SPOT)
    theta = CONTACT_THETA
    eye = (SPOT[0] - 200.0, SPOT[1] - 330.0, 300.0)
    first = dict(speed=CONTACT_SPEED, team=0, seed=11035, **MOP)

    def steps(length, splash):
        def frame(k):
            return k * length
        events = {
            0: [lab("view", "Islands"), lab("panel", "Atlas"), lab("panelChannel", "Islands"), lab("selected", 0)],
            frame(1): [lab("panel", "None"), lab("view", "Lit"),
                       fire(incident(target, theta, 1500.0), target, lead=0.2, splash=splash, **first)],
            frame(1) + length // 2: [lab("view", "Score")],
            frame(2): [lab("view", "Lit"),
                       shot_at(add(SPOT, (70.0, 50.0, 0.0)), theta, lead=0.15, speed=CONTACT_SPEED, team=1, seed=7, **MOP)],
            frame(3): [lab("panel", "ShapeLab"), splat("seed", 11035), splat("theta", theta), splat("shapeStage", 8.0)],
            frame(4): [lab("panel", "Atlas"), lab("panelChannel", "Profile"), lab("view", "Height")],
            frame(5): [lab("panel", "None"), lab("view", "Normal")],
            frame(5) + length // 2: [lab("view", "Lit")],
        }
        captions = [
            (0, "1 RenderTarget2D · island atlas"),
            (frame(1), "2 GPU 스탬프 | CPU 셀 점수"),
            (frame(2), "3 팀 블렌딩"),
            (frame(3), "4 스플랫 모양"),
            (frame(4), "5 높이 누적"),
            (frame(5), "6 노멀 → 최종"),
        ]
        if splash:
            events[frame(6)] = [lab("splashDebug", True),
                                shot_at(add(SPOT, (-80.0, -60.0, 0.0)), theta, lead=0.1, speed=CONTACT_SPEED, team=0, seed=12, splash=True, **MOP)]
            captions.append((frame(6), "7 스플래시"))
        return events, captions

    six_events, six_captions = steps(60, False)
    seven_events, seven_captions = steps(51, True)
    return [
        {
            "name": "e2e_1to6",
            "seconds": 6.0,
            "setup": camera(eye, SPOT, 50),
            "events": six_events,
            "captions": six_captions,
        },
        {
            "name": "e2e_1to7",
            "seconds": 6.0,
            "setup": camera(eye, SPOT, 50),
            "events": seven_events,
            "captions": seven_captions,
        },
    ]


def grid_tile(speed_index, theta_index):
    return (
        (0.5 * (GRID_SIDE - 1) - speed_index) * GRID_PITCH,
        (theta_index - 0.5 * (GRID_SIDE - 1)) * GRID_PITCH,
        0.0,
    )


def grid_volley():
    calls = []
    for si, v in enumerate(GRID_SPEED):
        for ti, th in enumerate(GRID_THETA):
            c = list(grid_tile(si, ti))
            calls.append(fire(incident(c, th, 900.0), c, speed=v, team=0, seed=2024, splash=True,
                              onlySurface=FIRST_TILE + si * GRID_SIDE + ti, lead=0.5))
    return calls


def grid_shots():
    shots = []
    for si in range(GRID_SIDE):
        for ti in range(GRID_SIDE):
            c = grid_tile(si, ti)
            shots.append({
                "name": f"grid_{si}{ti}",
                "seconds": 5.0,
                "fps": 60,
                "setup": [lab("stage", "Grid")] + camera((c[0], c[1] - 290.0, 330.0), (c[0], c[1] + 10.0, 0.0), 50),
                "settle": 3,
                "events": {0: grid_volley()},
            })
    return shots


def still_sheets():
    panel = [lab("panel", "ShapeLab"), splat("seed", 5), splat("theta", 50.0)]
    stages = [(f"stage{k}", [splat("shapeStage", float(k))]) for k in range(9)]
    sunflower = [("sunflower", [splat("theta", 0.0), splat("shapeStage", 3.0)])]
    seeds = [(f"seed{k:02d}", [splat("seed", 1000 + 37 * k)]) for k in range(64)]
    return [
        {"name": "stills_shape_stages", "setup": panel, "frames": stages + sunflower, "crop": PANEL_CROP},
        {"name": "stills_seed_sheet", "setup": panel[:1] + [splat("theta", 0.0), splat("shapeStage", 8.0)],
         "frames": seeds, "crop": (1256, 24, 640, 640),
         "montage": {"pattern": "seed%02d.png", "cell": 240, "tile": "8x8", "out": "seed_sheet.png"}},
    ]


def all_shots():
    return (t1_shots() + t2_shots() + t3_shots() + t4_shots() + t5_shots()
            + t6_shots() + t7_shots() + e2e_shots() + grid_shots() + still_sheets())
