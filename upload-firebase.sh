#!/bin/bash
set -euo pipefail
# Upload a signed Android APK to Firebase App Distribution.
# Usage: ./upload-firebase.sh [options] [path-to.apk]
# With no APK path, uploads the newest signed full APK in dist/.
# A locked Play build is refused. Run ./build-android.sh --full first.
#
# Options:
#   --groups <aliases>     Comma-separated tester groups to notify
#   --testers <emails>     Comma-separated tester emails to notify
#   --notes <text>         Release notes (default: the APK file name)
#   --notes-file <path>    Release notes from a file
#   --app <firebase-app-id>
#
# Sign in once with `firebase login`, or set FIREBASE_TOKEN from
# `firebase login:ci`, or set GOOGLE_APPLICATION_CREDENTIALS to a service
# account that can distribute this app. The token is not stored in the repo.
# FIREBASE_GROUPS and FIREBASE_TESTERS are used when the matching flag is omitted.
# With neither set, the build is shared with the testers already on the project.
# FIREBASE_APP_ID overrides the default app. FIREBASE_PROJECT_ID overrides the
# project looked up from that app.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP_ID="${FIREBASE_APP_ID:-1:199263152632:android:4db0f5d1770b2da1254e0d}"
# Not named GROUPS: bash uses that for the user's numeric group ids, so a
# plain ./upload-firebase.sh was sending --groups 1000 and Firebase returned 404.
GROUP_ALIASES="${FIREBASE_GROUPS:-}"
TESTERS="${FIREBASE_TESTERS:-}"
NOTES=""
NOTES_FILE=""
APK=""

usage() {
    grep '^#' "$0" | grep -v '^#!' | sed 's/^# \{0,1\}//'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --groups) GROUP_ALIASES="${2:-}"; shift 2 ;;
        --testers) TESTERS="${2:-}"; shift 2 ;;
        --notes) NOTES="${2:-}"; shift 2 ;;
        --notes-file) NOTES_FILE="${2:-}"; shift 2 ;;
        --app) APP_ID="${2:-}"; shift 2 ;;
        --) shift; break ;;
        -*) echo "Unknown argument: $1" >&2; usage >&2; exit 1 ;;
        *)
            if [ -n "$APK" ]; then
                echo "Only one APK path is accepted." >&2
                exit 1
            fi
            APK="$1"
            shift
            ;;
    esac
done

if [ -z "$APP_ID" ]; then
    echo "Set FIREBASE_APP_ID or pass --app." >&2
    exit 1
fi

if [ -z "$APK" ]; then
    if [ ! -d "$ROOT/dist" ]; then
        echo "No signed APK in dist/. Run ./build-android.sh first, or pass a path." >&2
        exit 1
    fi
    mapfile -t apks < <(find "$ROOT/dist" -maxdepth 1 -type f -name 'waterfall-*-full-arm64-v8a.apk' ! -name '*-unsigned.apk' -printf '%T@ %p\n' | sort -nr | cut -d' ' -f2-)
    if [ "${#apks[@]}" -eq 0 ]; then
        echo "No full APK in dist/. Run ./build-android.sh --full, or pass a path." >&2
        exit 1
    fi
    APK="${apks[0]}"
fi
if [ ! -f "$APK" ]; then
    echo "APK not found: $APK" >&2
    exit 1
fi
case "$APK" in
    *-unsigned.apk)
        echo "Refusing to upload an unsigned APK: $APK" >&2
        exit 1
        ;;
    *-full-arm64-v8a.apk)
        ;;
    *)
        echo "Refusing to upload a locked APK: $APK" >&2
        echo "Firebase builds are the full version. Run ./build-android.sh --full" >&2
        exit 1
        ;;
esac
if [ -n "$NOTES_FILE" ] && [ ! -f "$NOTES_FILE" ]; then
    echo "Release notes file not found: $NOTES_FILE" >&2
    exit 1
fi

FIREBASE="$(command -v firebase || true)"
if [ -z "$FIREBASE" ]; then
    echo "firebase not found. Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi

if [ -z "$NOTES" ] && [ -z "$NOTES_FILE" ]; then
    NOTES="${APK##*/}"
fi

# A missing group makes Firebase return 404 after the APK is already uploaded.
# Check aliases first. With no group and no tester list, share with the people
# already invited to the project.
project_number="${APP_ID#*:}"
project_number="${project_number%%:*}"
if [ -z "${FIREBASE_PROJECT_ID:-}" ]; then
    if ! PROJECT_ID="$("$FIREBASE" projects:list --json | python3 -c '
import json, sys
number = sys.argv[1]
for project in json.load(sys.stdin)["result"]:
    if str(project.get("projectNumber")) == number:
        print(project["projectId"])
        raise SystemExit
raise SystemExit(1)
' "$project_number")"; then
        echo "No Firebase project numbered ${project_number}. Set FIREBASE_PROJECT_ID." >&2
        exit 1
    fi
else
    PROJECT_ID="$FIREBASE_PROJECT_ID"
fi
echo "Firebase project: ${PROJECT_ID}"

if [ -n "$GROUP_ALIASES" ]; then
    "$FIREBASE" appdistribution:groups:list --project "$PROJECT_ID" --json \
        | python3 -c '
import json, sys
wanted = [part.strip() for part in sys.argv[1].split(",") if part.strip()]
groups = json.load(sys.stdin)["result"].get("groups") or []
known = []
for group in groups:
    alias = group.get("alias") or group.get("name", "").rsplit("/", 1)[-1]
    if alias:
        known.append(alias)
missing = [alias for alias in wanted if alias not in known]
if missing:
    print("No tester group: " + ", ".join(missing), file=sys.stderr)
    print("Firebase wants the group alias, not the display name.", file=sys.stderr)
    if known:
        print("Groups in this project: " + ", ".join(known), file=sys.stderr)
    else:
        print("This project has no tester groups.", file=sys.stderr)
    print("Omit --groups to share with the testers already on the project.", file=sys.stderr)
    raise SystemExit(1)
' "$GROUP_ALIASES"
fi

if [ -z "$GROUP_ALIASES" ] && [ -z "$TESTERS" ]; then
    testers_json="$(mktemp)"
    set +e
    "$FIREBASE" appdistribution:testers:list --project "$PROJECT_ID" --json >"$testers_json"
    testers_status=$?
    set -e
    TESTERS="$(python3 -c '
import json, sys
payload = json.load(sys.stdin)
result = payload.get("result") or {}
testers = result.get("testers") or []
emails = []
for tester in testers:
    email = tester.get("name", "").rsplit("/", 1)[-1]
    if "@" in email:
        emails.append(email)
print(",".join(emails))
' <"$testers_json")" || TESTERS=""
    rm -f "$testers_json"
    if [ "$testers_status" -ne 0 ] || [ -z "$TESTERS" ]; then
        echo "No testers in this project. The build will be uploaded and not shared."
        TESTERS=""
    fi
fi

args=(appdistribution:distribute "$APK" --app "$APP_ID")
if [ -n "$NOTES_FILE" ]; then
    args+=(--release-notes-file "$NOTES_FILE")
else
    args+=(--release-notes "$NOTES")
fi
if [ -n "$GROUP_ALIASES" ]; then
    args+=(--groups "$GROUP_ALIASES")
fi
if [ -n "$TESTERS" ]; then
    args+=(--testers "$TESTERS")
fi
if [ -n "${FIREBASE_TOKEN:-}" ]; then
    args+=(--token "$FIREBASE_TOKEN")
fi

if [ -z "${FIREBASE_TOKEN:-}" ] && [ -z "${GOOGLE_APPLICATION_CREDENTIALS:-}" ]; then
    echo "Using the Firebase CLI login. If upload is rejected, run: firebase login"
fi
if [ -n "$GROUP_ALIASES" ]; then
    echo "Sharing with groups: $GROUP_ALIASES"
fi
if [ -n "$TESTERS" ]; then
    echo "Sharing with testers: $TESTERS"
fi
echo "Uploading ${APK##*/} to Firebase app ${APP_ID}"
"$FIREBASE" "${args[@]}"
