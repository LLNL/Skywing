#ifndef DATA_INPUT
#define DATA_INPUT

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

#include <fstream>
#include <iostream>
#include <vector>
/**
 * This function inputs a vector or matrix from an unstructured and dense matrix market format.
 * In the future, this functionality may be expanded to other types and formats.
 */

template <typename T>
std::vector<std::vector<T>> input_matrix_from_matrix_market(std::string directory, std::string matrix_name)
{
  std::vector<std::vector<T>> return_mat;
  std::string file_path =  directory + "/" + matrix_name;
  std::ifstream fin(file_path, std::ifstream::in);
  assert(fin.is_open() == 1);
  int input_rows = 0;
  int input_cols = 0;
  // Ignore headers and comments in matrix market format.
  while (fin.peek() == '%')
  {
    fin.ignore(2048, '\n');
  }
  // this inputs the rows and cols
  fin >> input_rows >> input_cols ;
  return_mat.resize(input_rows);
  // This extra resize loop is due to how matrix market stacks columns on top of each other.
  for(int k=0; k<input_rows; k++)
  {
      return_mat[k].resize(input_cols);
  }
  double hold = 0.0;
  for(int i = 0 ; i < input_cols; i++)
  {
      for(int j = 0 ; j < input_rows; j++)
      {
          // in_file >> data;
          fin >> hold;
          return_mat[j][i] = hold;
      }
      hold =0.0;
  }
  fin.close();
  return return_mat;
}

template <typename T>
std::vector<T> input_vector_from_matrix_market(std::string directory, std::string vector_name)
{
  std::vector<T> return_vec;
  std::string file_path =  directory + "/" + vector_name;
  std::ifstream fin(file_path, std::ifstream::in);
  assert(fin.is_open() == 1);
  int input_rows = 0;
  int input_cols = 0;
  // Ignore headers and comments in matrix market format.
  while (fin.peek() == '%')
  {
    fin.ignore(2048, '\n');
  }
  fin >> input_rows >> input_cols ;
  if(input_rows ==1 || input_cols ==1)
  {
    int input_count = std::max(input_rows,input_cols);
    double hold = 0.0;
    for(int i = 0 ; i < input_count; i ++)
    {
      fin >> hold;
      return_vec.push_back(hold);
      hold = 0.0;
    }
  }
  else
  {
    std::cerr << "WARNING: This is not a vector: row number: "  << input_rows << "col number: " << input_cols << std::endl;
  }
  fin.close();
  return return_vec;
}

// Potentially useful terminal diagnostics.
template<typename T>
void print_mat(std::vector<std::vector<T>> print_me)
{
  for( auto row : print_me)
  {
    for( auto entry: row)
    {
      std::cout << entry << " ";
    }
    std::cout << std::endl;
  }
}

template<typename T>
void print_vec(std::vector<T> print_me)
{
  for( auto entry: print_me)
  {
    std::cout << entry << " " ;
  }
  std::cout << std::endl;
}

#endif
