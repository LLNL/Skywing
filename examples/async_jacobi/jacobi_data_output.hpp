double calculate_partial_residual(
    std::vector<double> x_local_solution,
    std::vector<double> b_partition,
    std::vector<std::vector<double>> A_partition)
{
  double partial_residual = 0.0;
  for (int i = 0 ; i < static_cast<double>(A_partition.size()); i++)
  {
    double partial_residual_hold = 0.0;
    for (int j = 0; j < static_cast<double>(A_partition[0].size()); j++)
    {
      partial_residual_hold = A_partition[i][j]*x_local_solution[j]+ partial_residual_hold;
    }
    partial_residual_hold = partial_residual_hold - b_partition[i];
    partial_residual_hold = pow(partial_residual_hold,2);
    partial_residual = partial_residual + partial_residual_hold;
  }
  return partial_residual;
}

double calculate_partial_forward_error(
    std::vector<size_t> row_indices,
    std::vector<double> x_partition_estimate,
    std::vector<double> x_local_solution)
{
  double partitioned_forward_error = 0.0;
  // std::vector<double> forward_error_vector(static_cast<int>(x_local_solution.size()),0.0);
  for(size_t i =0 ; i < row_indices.size(); i++)
  {
    double hold_value = 0.0;
    hold_value = x_partition_estimate[i]-x_local_solution[i];
    hold_value = pow(hold_value,2);
    partitioned_forward_error += hold_value;
  }
  return partitioned_forward_error;
}

double calculate_local_forward_error(std::vector<double> x_estimate, std::vector<double> x_full_solution)
{
  double full_forward_error = 0.0;
  assert(x_estimate.size() == x_full_solution.size());
  // std::vector<double> forward_error_vector(static_cast<int>(x_local_solution.size()),0.0);
  for(int i =0 ; i < static_cast<int> (x_estimate.size()) ; i++)
  {
    double hold_value = 0.0;
    hold_value = x_estimate[i]-x_full_solution[i];
    hold_value = pow(hold_value,2);
    full_forward_error += hold_value;
  }
  return full_forward_error;
}


void collect_data_each_component(int machine_number, int redundancy, int trial, double partial_forward_error, double partial_residual, int iteration_count, double time, std::string save_folder)
{
  std::string local_information_filename = save_folder + "/redundancy_" + std::to_string(redundancy) + "_trial_" + std::to_string(trial) + "_rank_" + std::to_string(machine_number) + ".csv";
  std::ofstream local_information;
  local_information.open(local_information_filename);
  local_information << "Redundancy ";
  local_information << ",";
  local_information << "Trial";
  local_information << ",";
  local_information << "Rank ";
  local_information << ",";
  local_information << "Local Error";
  local_information << ",";
  local_information << "Local Residual";
  local_information << ",";
  local_information << "Iteration Count";
  local_information << ",";
  local_information << "Time";
  local_information << "\n";

  local_information << redundancy;
  local_information << ",";
  local_information << trial;
  local_information << ",";
  local_information << machine_number;
  local_information << ",";
  local_information << partial_forward_error;
  local_information << ",";
  local_information << partial_residual;
  local_information << ",";
  local_information << iteration_count;
  local_information << ",";
  local_information << time;
  local_information << "\n";

  local_information.close();
}
