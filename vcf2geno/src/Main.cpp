#include <cassert>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>

#include "tabix.h"

#include "Argument.h"
#include "Utils.h"
#include "VCFUtil.h"
#include "SimpleMatrix.h"
#include "IO.h"

/**
 * All indices are 0-based
 */
void appendGeno(SimpleMatrix* geno, const int peopleIndex, const int genoIndex, int g) {
  if (geno->nrow() <= peopleIndex) {
    geno->resize(geno->nrow() + 1, geno->ncol());
  }
  if (geno->ncol() <= genoIndex) {
    geno->resize(geno->nrow(), geno->ncol() + 1);
  }
  (*geno)[peopleIndex][genoIndex] = g;
}

typedef std::map<std::string, std::vector<std::string> > IDUpdater;
int loadUpdateIDFile(const std::string& fn, IDUpdater* d) {
  if (fn.empty()) return 0;
  LineReader lr(fn);
  std::vector<std::string> fd;
  int lineNo = 0;
  IDUpdater& data = *d;
  while (lr.readLineBySep(&fd, " \t")){
    ++lineNo;
    if (fd.size() == 0) continue;
    if (fd.size() < 3) {
      fprintf(stderr,
              "Line %d should have at least 3 columns, %d columns detected. Skipped...\n",
              lineNo, (int)fd.size());
      continue;
    }
    
    if (data.count(fd[0])) {
      fprintf(stderr, "Line %d has duplicated id [ %s ]. Skipped...\n",
              lineNo, fd[0].c_str());
      continue;
    }
    data[fd[0]].push_back(fd[1]);
    data[fd[0]].push_back(fd[2]);
  }
  fprintf(stderr, "Loaded %d entries from %s.\n", (int)data.size(), fn.c_str());
  return 0;
}

bool isACGT(char c) {
  if (c == 'A' || c == 'C' || c == 'G' || c == 'T') return true;
  if (c == 'a' || c == 'c' || c == 'g' || c == 't') return true;
  return false;
}
/**
 * Check if the VCF site is a bi-allelic SNP site
 */
bool isBiallelicSite(const char* ref, const char* alt) {
  if (strlen(ref) != 1 ||
      strlen(alt) != 1) return false;
  if (isACGT(ref[0]) && isACGT(alt[0])) return true;
  return false;
}

int main(int argc, char** argv){
    time_t currentTime = time(0);
    fprintf(stderr, "Analysis started at: %s", ctime(&currentTime));

    ////////////////////////////////////////////////
    BEGIN_PARAMETER_LIST(pl)
        ADD_PARAMETER_GROUP(pl, "Input/Output")
        ADD_STRING_PARAMETER(pl, inVcf, "--inVcf", "input VCF File")
        ADD_STRING_PARAMETER(pl, outPrefix, "--out", "output prefix")
        ADD_PARAMETER_GROUP(pl, "People Filter")
        ADD_STRING_PARAMETER(pl, peopleIncludeID, "--peopleIncludeID", "give IDs of people that will be included in study")
        ADD_STRING_PARAMETER(pl, peopleIncludeFile, "--peopleIncludeFile", "from given file, set IDs of people that will be included in study")
        ADD_STRING_PARAMETER(pl, peopleExcludeID, "--peopleExcludeID", "give IDs of people that will be included in study")
        ADD_STRING_PARAMETER(pl, peopleExcludeFile, "--peopleExcludeFile", "from given file, set IDs of people that will be included in study")
        ADD_PARAMETER_GROUP(pl, "Site Filter")
        ADD_STRING_PARAMETER(pl, rangeList, "--rangeList", "Specify some ranges to use, please use chr:begin-end format.")
        ADD_STRING_PARAMETER(pl, rangeFile, "--rangeFile", "Specify the file containing ranges,please use chr:begin-end format.")
        ADD_PARAMETER_GROUP(pl, "Auxilary Function")
        ADD_BOOL_PARAMETER(pl, keepDuplication, "--keepDuplication", "Output all variants (by default, we keep one variant out of all duplicated sites, chr:pos)")
        ADD_STRING_PARAMETER(pl, updateID, "--updateID", "Specify a three column file where the first column is the IDs in VCF input file, and the second, third columns are family and individual IDs in the output files.")
        END_PARAMETER_LIST(pl)
        ;    

    pl.Read(argc, argv);
    pl.Status();
    
    if (FLAG_REMAIN_ARG.size() > 0){
        fprintf(stderr, "Unparsed arguments: ");
        for (unsigned int i = 0; i < FLAG_REMAIN_ARG.size(); i++){
            fprintf(stderr, " %s", FLAG_REMAIN_ARG[i].c_str());
        }
        fprintf(stderr, "\n");
        abort();
    }

    REQUIRE_STRING_PARAMETER(FLAG_inVcf, "Please provide input file using: --inVcf");

    // load id updater file
    IDUpdater idUpdater;
    loadUpdateIDFile(FLAG_updateID, &idUpdater);
    
    const char* fn = FLAG_inVcf.c_str(); 
    VCFInputFile vin(fn);

    // set range filters here
    // e.g.     
    // vin.setRangeList("1:69500-69600");
    vin.setRangeList(FLAG_rangeList.c_str());
    vin.setRangeFile(FLAG_rangeFile.c_str());

    // set people filters here
    if (FLAG_peopleIncludeID.size() || FLAG_peopleIncludeFile.size()) {
        vin.excludeAllPeople();
        vin.includePeople(FLAG_peopleIncludeID.c_str());
        vin.includePeopleFromFile(FLAG_peopleIncludeFile.c_str());
    }
    vin.excludePeople(FLAG_peopleExcludeID.c_str());
    vin.excludePeopleFromFile(FLAG_peopleExcludeFile.c_str());

    // store intemediate results
    OrderedMap < std::string, int> markerIndex;
    SimpleMatrix geno; // store genotypes by person
    
    // let's write it out.
    VCFOutputFile* vout = NULL;

    // if (FLAG_updateId != "") {
    //   int ret = vin.updateId(FLAG_updateId.c_str());
    //   fprintf(stdout, "%d samples have updated id.\n", ret);
    // }
    FILE* fSite = fopen( (FLAG_outPrefix + ".site").c_str(), "wt");
    fprintf(fSite, "CHROM\tPOS\tID\tREF\tALT\n");
    std::string markerName;
    int lineNo = 0;
    // int nonVariantSite = 0;
    while (vin.readRecord()){
        lineNo ++;
        VCFRecord& r = vin.getVCFRecord(); 
        VCFPeople& people = r.getPeople();
        VCFIndividual* indv;
        // if (FLAG_variantOnly) {
        //   bool hasVariant = false;
        //   int geno;
        //   int GTidx = r.getFormatIndex("GT");
        //   for (int i = 0; i < people.size() ;i ++) {
        //     indv = people[i];
        //     geno = indv->justGet(0).getGenotype();
        //     if (geno != 0 && geno != MISSING_GENOTYPE)
        //       hasVariant = true;
        //   }
        //   if (!hasVariant) {
        //     nonVariantSite++;
        //     continue;
        //   }
        // }
        if (!isBiallelicSite(r.getRef(), r.getAlt())) {
          fprintf(stderr, "Skip non-biallelic variant site: [ %s\t%s\t%s\t%s\t%s ]\n",
                  r.getChrom(),
                  r.getPosStr(),
                  r.getID(),
                  r.getRef(),
                  r.getAlt());
          continue;
        }
        markerName = r.getChrom();
        markerName += ":";
        markerName += r.getPosStr();

        /// fprintf(stderr, "process %s\n", markerName.c_str());
        if (markerIndex.find(markerName) &&
            !FLAG_keepDuplication) {
          fprintf(stderr, "Skip duplicated variant site:  [ %s\t%s\t%s\t%s\t%s ]\n",
                  r.getChrom(),
                  r.getPosStr(),
                  r.getID(),
                  r.getRef(),
                  r.getAlt());
          continue;
        } 
        const int index = markerIndex.size();
        markerIndex[markerName] = index;
        
        fprintf(fSite, "%s\t%s\t%s\t%s\t%s\n", r.getChrom(), r.getPosStr(), markerName.c_str(), r.getRef(), r.getAlt());
        
        int GTidx = r.getFormatIndex("GT");
        bool missing;
        for (int i = 0; i < people.size(); ++i) {
          const VCFValue& v = people[i]->get(GTidx, &missing);
          if (!missing) {
            // only count reference alleles
            int g1 = v.getAllele1();
            int g2 = v.getAllele2();
            if (g1 < 0 || g2 < 0) {
              appendGeno(&geno, i, index, -9);  
            } else {
              appendGeno(&geno, i, index, (g1 == 0 ? 1 : 0) + (g2 == 0 ? 1 : 0));  
            }
          } else {
            appendGeno(&geno, i, index, -9);
          }
        }
    };
    fclose(fSite);
    
    // output geno file
    FILE* fGeno = fopen ( (FLAG_outPrefix + ".geno").c_str(), "wt");
    std::vector<std::string> names;
    vin.getVCFHeader()->getPeopleName(&names);
    for (int i = 0; i < names.size(); ++i) {
      // update id
      if (idUpdater.empty()) {
        fprintf(fGeno, "%s\t%s", names[i].c_str(), names[i].c_str());
      } else {
        if (idUpdater.count(names[i]) == 0) {
          fprintf(stderr, "VCF ID [ %s ] not found in idUpdater file\n", names[i].c_str());
          fprintf(fGeno, "%s\t%s", names[i].c_str(), names[i].c_str());
        } else {
          fprintf(fGeno, "%s\t%s",
                  idUpdater[names[i]][0].c_str(),
                  idUpdater[names[i]][1].c_str());
        }
      }
      // output genotype
      for (int j = 0; j < geno.ncol(); ++j) {
        fprintf(fGeno, "\t%d", (int) geno[i][j]);
      }
      fprintf(fGeno, "\n");
    }
    fclose(fGeno);
    
    fprintf(stdout, "Total %d VCF records have converted successfully\n", lineNo);
    fprintf(stdout, "Total %d people and %d markers are outputted\n", geno.nrow(), geno.ncol());
    // if (FLAG_variantOnly) {
    //   fprintf(stdout, "Skipped %d non-variant VCF records\n", nonVariantSite);
    // }
    
    return 0; 
};
