/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   microshell.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vfurmane <vfurmane@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/01/18 14:29:04 by vfurmane          #+#    #+#             */
/*   Updated: 2022/01/31 09:37:20 by vfurmane         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct s_cmd
{
	int		pid;
	int		ifd;
	int		ofd;
	char	**args;
	bool	last_of_pipes;
	bool	last_of_commands;
}				t_cmd;

size_t	ft_strlen(const char *str)
{
	int	i = 0;
	while (str[i])
		i++;
	return i;
}

size_t	is_a_separator(const char *str)
{
	return ft_strlen(str) == 1 && (str[0] == ';' || str[0] == '|');
}

size_t	number_of_commands(int argc, char **argv)
{
	size_t	count = 1;
	for (int i = 1; i < argc - 1; i++)
	{
		if (is_a_separator(argv[i]))
			count++;
	}
	return count;
}

size_t	number_of_arguments(int argc, char **argv)
{
	size_t	i = 0;
	while (i < (size_t)argc && !is_a_separator(argv[i]))
		i++;
	return i;
}

t_cmd	*parse_command_line(int argc, char **argv)
{
	t_cmd	*commands;
	size_t	commands_nbr = number_of_commands(argc, argv);
	size_t	arguments_nbr;
	int		cmd_i = 0;
	int		arg_i = 0;

	// printf("nbr of commands: %zu\n", commands_nbr);
	commands = malloc(sizeof *commands * (commands_nbr + 1));
	if (commands == NULL)
		return NULL;
	for (int i = 1; i < argc; i++)
	{
		if (arg_i == 0)
		{
			arguments_nbr = number_of_arguments(argc - i, &argv[i]);
			// printf(" nbr of arguments: %zu\n", arguments_nbr);
			commands[cmd_i].args = malloc(sizeof *commands[cmd_i].args * (arguments_nbr + 1));
			if (commands[cmd_i].args == NULL)
				return NULL;
			commands[cmd_i].ifd = 0;
			commands[cmd_i].ofd = 1;
			commands[cmd_i].last_of_pipes = false;
			commands[cmd_i].last_of_commands = false;
			commands[cmd_i].args[arguments_nbr] = NULL;
		}
		if (ft_strlen(argv[i]) == 1 && argv[i][0] == ';')
			commands[cmd_i].last_of_pipes = true;
		if (is_a_separator(argv[i]))
		{
			cmd_i++;
			arg_i = 0;
		}
		else
		{
			commands[cmd_i].last_of_pipes = false;
			commands[cmd_i].args[arg_i++] = argv[i];
		}
	}
	commands[commands_nbr - 1].last_of_commands = true;
	return commands;
}

int	fork_and_exec(t_cmd *commands, int i, char **envp)
{
	int	ret = 0;
	int	status;
	int	pipefd[2];
	const bool is_piped = !commands[i].last_of_pipes && !commands[i].last_of_commands;

	if (is_piped)
	{
		pipe(pipefd);
		commands[i].ofd = pipefd[1];
		commands[i + 1].ifd = pipefd[0];
	}
	const int	id = fork();
	if (id == 0)
	{
		// printf("Executing %s (%d|%d)...\n", commands[i].args[0], commands[i].ifd, commands[i].ofd);
		if (is_piped)
			close(pipefd[0]);
		dup2(commands[i].ifd, STDIN_FILENO);
		dup2(commands[i].ofd, STDOUT_FILENO);
		if (execve(commands[i].args[0], commands[i].args, envp) == -1)
		{
			write(2, "execve error\n", 13);
			exit(1);
		}
	}
	else
	{
		if (is_piped)
			close(pipefd[1]);
		commands[i].pid = id;
		if (!commands[i].last_of_commands && !commands[i].last_of_pipes)
			ret = fork_and_exec(commands, i + 1, envp);
		waitpid(commands[i].pid, &status, 0);
		if (commands[i].ifd != STDIN_FILENO)
			close(commands[i].ifd);
		if (commands[i].ofd != STDOUT_FILENO)
			close(commands[i].ofd);
		if (!commands[i].last_of_commands)
			return ret;
		else
		{
			if (WIFEXITED(status))
				return WEXITSTATUS(status);
			else if (WIFSIGNALED(status))
				return 128 + WTERMSIG(status);
		}
	}
	return -1;
}

int	exec_commands(t_cmd *commands, char **envp)
{
	int	i = -1;
	int	ret;

	do
	{
		i++;
		ret = fork_and_exec(commands, i, envp);
		while (!commands[i].last_of_commands && !commands[i].last_of_pipes)
			i++;
	} while (!commands[i].last_of_commands);
	return ret;
}

int	main(int argc, char **argv, char **envp)
{
	int		i = 0;
	int		ret = 0;
	t_cmd	*commands;

	commands = parse_command_line(argc, argv);
	if (commands == NULL)
		return 1;
	if (argc > 1)
	{
		ret = exec_commands(commands, envp);
		do
		{
			free(commands[i].args);
		} while (!commands[i++].last_of_commands);
	}
	free(commands);
	return ret;
}
