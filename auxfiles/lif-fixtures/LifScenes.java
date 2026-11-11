// Writes the Bio-Formats series reference the LIFCorpus tests compare scenes against.
//
//   java -cp bioformats_package.jar auxfiles/lif-fixtures/LifScenes.java <lif-dir> <out.tsv>
//
// <lif-dir> is $SLIDEIO_IMAGES_PATH/lif. Every .lif below it (sorted by path) is opened
// with Bio-Formats' LIFReader, and one row is written per series: the file relative to
// <lif-dir> with '/' separators, the series index, the OME image name, the core
// dimensions, the pixel type, and the physical pixel sizes in micrometres (empty when
// Bio-Formats reports none). Modelled on LifCheck.java of the LIF validation folder.
// Output is deterministic: LF line endings, Double.toString for the sizes.
import loci.common.DebugTools;
import loci.formats.FormatTools;
import loci.formats.MetadataTools;
import loci.formats.in.LIFReader;
import loci.formats.meta.IMetadata;
import ome.units.UNITS;
import ome.units.quantity.Length;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class LifScenes {
    static String um(Length l) {
        if (l == null) return "";
        Number v = l.value(UNITS.MICROMETER);
        return v == null ? "" : Double.toString(v.doubleValue());
    }

    static String name(String n) {
        return n == null ? "" : n;
    }

    public static void main(String[] args) throws Exception {
        DebugTools.setRootLevel("ERROR");
        Path root = Path.of(args[0]);
        List<Path> files;
        try (var paths = Files.walk(root)) {
            files = paths.filter(p -> p.toString().endsWith(".lif")).sorted().toList();
        }
        int rows = 0;
        try (PrintWriter out = new PrintWriter(Files.newBufferedWriter(Path.of(args[1]), StandardCharsets.UTF_8))) {
            out.print("file\tseries\tname\twidth\theight\tc\tz\tt\tpixelType\tphysX_um\tphysY_um\tphysZ_um\n");
            for (Path p : files) {
                String rel = root.relativize(p).toString().replace('\\', '/');
                IMetadata meta = MetadataTools.createOMEXMLMetadata();
                try (LIFReader r = new LIFReader()) {
                    r.setMetadataStore(meta);
                    r.setId(p.toString());
                    for (int s = 0; s < r.getSeriesCount(); s++) {
                        r.setSeries(s);
                        out.print(String.join("\t", rel, Integer.toString(s), name(meta.getImageName(s)),
                            Integer.toString(r.getSizeX()), Integer.toString(r.getSizeY()),
                            Integer.toString(r.getSizeC()), Integer.toString(r.getSizeZ()),
                            Integer.toString(r.getSizeT()),
                            FormatTools.getPixelTypeString(r.getPixelType()),
                            um(meta.getPixelsPhysicalSizeX(s)), um(meta.getPixelsPhysicalSizeY(s)),
                            um(meta.getPixelsPhysicalSizeZ(s))) + "\n");
                        rows++;
                    }
                }
            }
        }
        System.out.println(files.size() + " files, " + rows + " series");
    }
}
