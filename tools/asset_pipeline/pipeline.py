"""Preprocess one Alien Shooter package into tools/output."""

import argparse
import importlib.metadata
import logging
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv
import zipfile

TOOLS = Path(__file__).resolve().parents[1]
OUTPUT = TOOLS / "output"
ENVIRONMENT = OUTPUT / ".venv"
PYTHON = ENVIRONMENT / "Scripts/python.exe"


def prepare_environment(package):
    """Keep the Python environment and download cache inside output."""
    sys.dont_write_bytecode = True
    os.environ["PYTHONDONTWRITEBYTECODE"] = "1"
    os.environ["PIP_CACHE_DIR"] = str(OUTPUT / ".cache/pip")
    if Path(sys.executable).resolve() != PYTHON.resolve():
        if not PYTHON.is_file():
            logging.info("Preparing Python environment")
            venv.EnvBuilder(with_pip=True).create(ENVIRONMENT)
        return subprocess.call([str(PYTHON), "-B", str(Path(__file__).resolve()), str(package)])
    requirements = TOOLS / "requirements.txt"
    for line in requirements.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        name, version = line.split("==")
        try:
            installed = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            installed = None
        if installed != version:
            logging.info("Installing pipeline dependencies")
            subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(requirements)], check=True)
            break
    return None


def unpack_archive(package, destination):
    """Extract a ZIP or JAR without allowing paths outside its destination."""
    destination = destination.resolve()
    with zipfile.ZipFile(package) as archive:
        for item in archive.infolist():
            target = (destination / item.filename).resolve()
            if ":" in item.filename or not target.is_relative_to(destination):
                raise ValueError(f"Invalid archive path: {item.filename}")
            if item.is_dir():
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)


def find_resources(directory):
    """Locate the game's resource folder beneath optional wrapper folders."""
    roots = []
    for path in directory.rglob("hd.png"):
        roots.append(path.parent)
    if len(roots) != 1:
        raise ValueError("Expected one Alien Shooter resource folder containing hd.png")
    return roots[0]


def clear_output():
    """Replace generated results while retaining the local environment and downloads."""
    for path in OUTPUT.iterdir():
        if path.name in (".venv", ".cache"):
            continue
        if not path.resolve().is_relative_to(OUTPUT):
            raise ValueError(f"Output entry points outside output: {path}")
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()


def build(package):
    """Copy the selected game resources and run the extraction once."""
    source = None
    if package.is_dir():
        source = find_resources(package)
    clear_output()
    logging.getLogger().addHandler(logging.FileHandler(OUTPUT / "pipeline.log", encoding="utf-8"))
    if source is None:
        unpacked = OUTPUT / ".unpacked"
        unpack_archive(package, unpacked)
        source = find_resources(unpacked)
    logging.info("Reading package: %s", package)
    shutil.copytree(source, OUTPUT / "source")
    if (OUTPUT / ".unpacked").exists():
        shutil.rmtree(OUTPUT / ".unpacked")
    sys.path.insert(0, str(TOOLS))
    from asset_pipeline.extract_assets import main as extract
    extract()
    logging.info("Output ready: %s", OUTPUT)


def main():
    """Accept exactly one package path."""
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="Alien Shooter directory, JAR or ZIP")
    args = parser.parse_args()
    try:
        if sys.version_info < (3, 10):
            raise ValueError("Python 3.10 or newer is required")
        package = args.package.expanduser().resolve()
        if not package.exists():
            raise ValueError(f"Package does not exist: {package}")
        if not package.is_dir() and package.suffix.lower() not in (".jar", ".zip"):
            raise ValueError("Input must be a directory, JAR or ZIP")
        if OUTPUT.is_symlink() or OUTPUT.resolve() != OUTPUT:
            raise ValueError("The output directory must be a local folder beside run_pipeline.bat")
        if package.is_relative_to(OUTPUT) or OUTPUT.is_relative_to(package):
            raise ValueError("The package and output must not contain each other")
        OUTPUT.mkdir(exist_ok=True)
        status = prepare_environment(package)
        if status is not None:
            return status
        build(package)
        return 0
    except (ValueError, OSError, RuntimeError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        logging.error("Pipeline failed: %s", error)
        return 1


if __name__ == "__main__":
    sys.exit(main())
