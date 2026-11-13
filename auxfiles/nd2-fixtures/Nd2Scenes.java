// Writes the Bio-Formats series reference the ND2Corpus tests compare scenes against.
//
//   java -cp bioformats_package.jar auxfiles/nd2-fixtures/Nd2Scenes.java <nd2-dir> <out.tsv>
//
// <nd2-dir> is $SLIDEIO_IMAGES_PATH/nd2. Every .nd2 below it (sorted by path) is opened
// with Bio-Formats' ND2Reader, and one row is written per series: the file relative to
// <nd2-dir> with '/' separators, the series index, the core dimensions (c is
// Bio-Formats' SizeC, RGB samples included), the pixel type, the physical pixel sizes in
// micrometres, a time increment in seconds and a magnification (each empty when
// Bio-Formats reports none).
//
// ND2Reader sets neither Pixels.TimeIncrement nor Objective.NominalMagnification, so
// those two columns are derived from what it does set:
//   tIncrement_s   (DeltaT(t = T-1) - DeltaT(t = 0)) / (T - 1) over the planes with
//                  c = 0, z = 0 of the series: the mean step of the recorded plane times.
//                  Empty when T < 2 or a DeltaT is missing.
//   magnification  NominalMagnification when set, else CalibratedMagnification of the
//                  objective the image's ObjectiveSettings names; empty when <= 0.
//
// Files Bio-Formats cannot open, and files whose bits per pixel exceed the storage width
// of their pixel type (jonas/divisionByZero/290110.tranMgc005cr.nd2: 186 bits in uint8,
// read from garbage attributes), are left out and named on stderr: there is nothing valid
// to compare with. Modelled on Nd2Check.java of the ND2 validation folder and on
// auxfiles/lif-fixtures/LifScenes.java. Output is deterministic: LF line endings,
// Double.toString for the numbers.
import loci.common.DebugTools;
import loci.formats.FormatTools;
import loci.formats.MetadataTools;
import loci.formats.in.ND2Reader;
import loci.formats.meta.IMetadata;
import ome.units.UNITS;
import ome.units.quantity.Length;
import ome.units.quantity.Time;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class Nd2Scenes {
    static String um(Length l) {
        if (l == null) return "";
        Number v = l.value(UNITS.MICROMETER);
        return v == null ? "" : Double.toString(v.doubleValue());
    }

    static Double seconds(Time t) {
        if (t == null) return null;
        Number v = t.value(UNITS.SECOND);
        return v == null ? null : v.doubleValue();
    }

    // Mean step of the plane times at c = 0, z = 0 (see the header).
    static String timeIncrement(ND2Reader r, IMetadata meta, int series) {
        int t = r.getSizeT();
        if (t < 2) return "";
        Double first = null, last = null;
        for (int p = 0; p < meta.getPlaneCount(series); p++) {
            int[] zct = r.getZCTCoords(p);
            if (zct[0] != 0 || zct[1] != 0) continue;
            if (zct[2] == 0) first = seconds(meta.getPlaneDeltaT(series, p));
            if (zct[2] == t - 1) last = seconds(meta.getPlaneDeltaT(series, p));
        }
        if (first == null || last == null) return "";
        return Double.toString((last - first) / (t - 1));
    }

    // The nominal magnification of the objective the image's ObjectiveSettings names.
    static String magnification(IMetadata meta, int series) {
        String id;
        try {
            id = meta.getObjectiveSettingsID(series);
        } catch (RuntimeException e) {
            return "";
        }
        if (id == null) return "";
        for (int i = 0; i < meta.getInstrumentCount(); i++) {
            for (int o = 0; o < meta.getObjectiveCount(i); o++) {
                if (id.equals(meta.getObjectiveID(i, o))) {
                    Double m = meta.getObjectiveNominalMagnification(i, o);
                    if (m == null) m = meta.getObjectiveCalibratedMagnification(i, o);
                    return m == null || m <= 0 ? "" : Double.toString(m);
                }
            }
        }
        return "";
    }

    public static void main(String[] args) throws Exception {
        DebugTools.setRootLevel("ERROR");
        Path root = Path.of(args[0]);
        List<Path> files;
        try (var paths = Files.walk(root)) {
            files = paths.filter(p -> p.toString().endsWith(".nd2")).sorted().toList();
        }
        int written = 0, rows = 0;
        try (PrintWriter out = new PrintWriter(Files.newBufferedWriter(Path.of(args[1]), StandardCharsets.UTF_8))) {
            out.print("file\tseries\twidth\theight\tc\tz\tt\tpixelType\tphysX_um\tphysY_um\tphysZ_um"
                + "\ttIncrement_s\tmagnification\n");
            for (Path p : files) {
                String rel = root.relativize(p).toString().replace('\\', '/');
                IMetadata meta = MetadataTools.createOMEXMLMetadata();
                try (ND2Reader r = new ND2Reader()) {
                    r.setMetadataStore(meta);
                    r.setId(p.toString());
                    int storage = 8 * FormatTools.getBytesPerPixel(r.getPixelType());
                    if (r.getBitsPerPixel() > storage) {
                        System.err.println("left out (" + r.getBitsPerPixel() + " bits in "
                            + FormatTools.getPixelTypeString(r.getPixelType()) + "): " + rel);
                        continue;
                    }
                    StringBuilder sb = new StringBuilder();
                    for (int s = 0; s < r.getSeriesCount(); s++) {
                        r.setSeries(s);
                        sb.append(String.join("\t", rel, Integer.toString(s),
                            Integer.toString(r.getSizeX()), Integer.toString(r.getSizeY()),
                            Integer.toString(r.getSizeC()), Integer.toString(r.getSizeZ()),
                            Integer.toString(r.getSizeT()),
                            FormatTools.getPixelTypeString(r.getPixelType()),
                            um(meta.getPixelsPhysicalSizeX(s)), um(meta.getPixelsPhysicalSizeY(s)),
                            um(meta.getPixelsPhysicalSizeZ(s)), timeIncrement(r, meta, s),
                            magnification(meta, s))).append('\n');
                        rows++;
                    }
                    out.print(sb);
                    written++;
                } catch (Exception e) {
                    System.err.println("left out (" + e + "): " + rel);
                }
            }
        }
        System.out.println(files.size() + " files, " + written + " written, " + rows + " series");
    }
}
