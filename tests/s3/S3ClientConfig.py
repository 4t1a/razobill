#!/usr/bin/env python3
"""
S3ClientConfig.py - Production AWS & Local S3 Configuration for DuckDB
========================================================================

Provides unified, robust S3 connectivity for DuckDB across:
1. Real AWS S3 (EC2, ECS, EKS, Lambda, AWS CLI credentials, IAM STS tokens).
2. LocalStack AWS Emulator (Docker port 4566 or AWS_ENDPOINT_URL).
3. MinIO Object Storage (port 9000 or custom endpoint).
4. Zero-Dependency MockS3Server (automated fallback for offline testing).
"""

import sys
import os
import socket
import urllib.request
from pathlib import Path
from typing import Optional, Tuple, Any

# Ensure tests/s3 is on path to find MockS3Server
CURRENT_DIR = Path(__file__).resolve().parent
if str(CURRENT_DIR) not in sys.path:
    sys.path.insert(0, str(CURRENT_DIR))

try:
    from MockS3Server import MockS3Server
except ImportError:
    MockS3Server = None


def isPortOpen(host: str, port: int) -> bool:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(0.5)
        ok = s.connect_ex((host, port)) == 0
        s.close()
        return ok
    except Exception:
        return False


def isRealAwsEnvironment() -> bool:
    """
    Detects whether the environment is running against live AWS S3.
    """
    endpoint = os.environ.get("AWS_ENDPOINT_URL", "").lower()
    if endpoint and (
        "localhost" in endpoint or "127.0.0.1" in endpoint or "localstack" in endpoint
    ):
        return False

    # Check for AWS compute runtime indicators
    if any(
        k in os.environ
        for k in (
            "AWS_EXECUTION_ENV",
            "ECS_CONTAINER_METADATA_URI",
            "AWS_LAMBDA_FUNCTION_NAME",
        )
    ):
        return True

    # Check if real AWS credentials exist without local emulator override
    access_key = os.environ.get("AWS_ACCESS_KEY_ID", "")
    if access_key and access_key not in ("test", "mock", "minioadmin") and not endpoint:
        return True

    return False


def configureDuckDbS3(
    con: Any,
    bucket: str = "telemetry",
    endpointOverride: Optional[str] = None,
    regionOverride: Optional[str] = None,
) -> Tuple[str, str, bool, Optional[Any]]:
    """
    Configures DuckDB's httpfs extension for S3 access.
    Returns:
        (bucket_name, endpoint_description, is_real_aws, mock_server_instance)
    """
    # 1. Load httpfs
    con.execute("INSTALL httpfs; LOAD httpfs;")

    # 2. Resolve Environment Variables
    env_endpoint = endpointOverride or os.environ.get("AWS_ENDPOINT_URL", "").strip()
    env_bucket = (
        os.environ.get("S3_BUCKET", "").strip()
        or os.environ.get("RAZOBILL_S3_BUCKET", "").strip()
    )
    if env_bucket:
        bucket = env_bucket

    region = (
        regionOverride
        or os.environ.get("AWS_REGION")
        or os.environ.get("AWS_DEFAULT_REGION", "us-east-1")
    )
    access_key = os.environ.get("AWS_ACCESS_KEY_ID", "").strip()
    secret_key = os.environ.get("AWS_SECRET_ACCESS_KEY", "").strip()
    session_token = os.environ.get("AWS_SESSION_TOKEN", "").strip()

    is_real_aws = False
    mock_server = None
    endpoint_desc = ""

    # Case A: Explicit or Detected Real AWS S3
    if env_endpoint in ("aws", "real-aws", "s3.amazonaws.com") or (
        isRealAwsEnvironment() and not env_endpoint
    ):
        is_real_aws = True
        endpoint_desc = f"AWS S3 Production ({region})"
        print(f"[*] Configuring DuckDB for Live AWS S3 in region '{region}'...")
        con.execute(f"SET s3_region = '{region}';")
        con.execute("SET s3_use_ssl = true;")
        con.execute("SET s3_url_style = 'vhost';")
        if access_key and secret_key:
            con.execute(f"SET s3_access_key_id = '{access_key}';")
            con.execute(f"SET s3_secret_access_key = '{secret_key}';")
        if session_token:
            con.execute(f"SET s3_session_token = '{session_token}';")
        return bucket, endpoint_desc, is_real_aws, None

    # Case B: Explicit Custom Endpoint (e.g. http://localhost:4566)
    if env_endpoint and env_endpoint not in ("mock", "none"):
        clean_endpoint = (
            env_endpoint.replace("http://", "").replace("https://", "").rstrip("/")
        )
        use_ssl = env_endpoint.startswith("https://")
        endpoint_desc = f"Custom S3 Endpoint ({env_endpoint})"
        print(
            f"[*] Configuring DuckDB for custom S3 endpoint: {clean_endpoint} (SSL={use_ssl})..."
        )
        con.execute(f"SET s3_endpoint = '{clean_endpoint}';")
        con.execute(f"SET s3_use_ssl = {'true' if use_ssl else 'false'};")
        con.execute("SET s3_url_style = 'path';")
        con.execute(f"SET s3_region = '{region}';")
        ak = access_key if access_key else "test"
        sk = secret_key if secret_key else "test"
        con.execute(f"SET s3_access_key_id = '{ak}';")
        con.execute(f"SET s3_secret_access_key = '{sk}';")
        return bucket, endpoint_desc, is_real_aws, None

    # Case C: Active LocalStack Port 4566
    if isPortOpen("127.0.0.1", 4566):
        endpoint_desc = "LocalStack AWS Emulator (127.0.0.1:4566)"
        print("[*] Detected active LocalStack AWS Emulator on 127.0.0.1:4566.")
        # Ensure bucket exists in LocalStack
        try:
            req = urllib.request.Request(
                f"http://127.0.0.1:4566/{bucket}", method="PUT"
            )
            urllib.request.urlopen(req, timeout=1.0)
        except Exception:
            pass
        con.execute("SET s3_endpoint = '127.0.0.1:4566';")
        con.execute("SET s3_use_ssl = false;")
        con.execute("SET s3_url_style = 'path';")
        con.execute(f"SET s3_region = '{region}';")
        ak = access_key if access_key else "test"
        sk = secret_key if secret_key else "test"
        con.execute(f"SET s3_access_key_id = '{ak}';")
        con.execute(f"SET s3_secret_access_key = '{sk}';")
        return bucket, endpoint_desc, is_real_aws, None

    # Case D: Active MinIO Port 9000
    if isPortOpen("127.0.0.1", 9000):
        endpoint_desc = "MinIO Object Storage (127.0.0.1:9000)"
        print("[*] Detected active MinIO service on 127.0.0.1:9000.")
        con.execute("SET s3_endpoint = '127.0.0.1:9000';")
        con.execute("SET s3_use_ssl = false;")
        con.execute("SET s3_url_style = 'path';")
        con.execute(f"SET s3_region = '{region}';")
        ak = access_key if access_key else "minioadmin"
        sk = secret_key if secret_key else "minioadmin"
        con.execute(f"SET s3_access_key_id = '{ak}';")
        con.execute(f"SET s3_secret_access_key = '{sk}';")
        return bucket, endpoint_desc, is_real_aws, None

    # Case E: Automated MockS3Server Fallback
    if MockS3Server is not None:
        print(
            "[*] Launching zero-dependency MockS3Server for local offline validation..."
        )
        mock_server = MockS3Server(host="127.0.0.1", port=9000)
        port = mock_server.start()
        endpoint_desc = f"MockS3Server (127.0.0.1:{port})"
        con.execute(f"SET s3_endpoint = '127.0.0.1:{port}';")
        con.execute("SET s3_use_ssl = false;")
        con.execute("SET s3_url_style = 'path';")
        con.execute(f"SET s3_region = '{region}';")
        con.execute("SET s3_access_key_id = 'mock';")
        con.execute("SET s3_secret_access_key = 'mock';")
        return bucket, endpoint_desc, is_real_aws, mock_server

    raise RuntimeError("No S3 endpoint detected and MockS3Server is unavailable.")
